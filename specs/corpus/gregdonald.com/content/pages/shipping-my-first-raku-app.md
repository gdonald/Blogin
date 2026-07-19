---
title: Shipping My First Raku App
date: 2026-05-25
slug: shipping-my-first-raku-app
description: Shipping My First Raku App: behave.dev I just released behave.dev into production. It's the homepage for BDD::Behave , my behavior-driven testing framework for Raku. It's the first Raku application...
tags: [behave, cro, haml, linux, raku, systemd]
archives: [2026-05]
---
**Shipping My First Raku App: behave.dev**

I just released [behave.dev](https://behave.dev/) into production. It's the homepage for [BDD::Behave](https://github.com/gdonald/BDD-Behave), my behavior-driven testing framework for Raku. It's the first Raku application I've ever pushed to production. I've been deploying web apps since 1996, so the moving parts of putting a process behind a reverse proxy on a Linux box are familiar territory. What I had to figure out this time was where the Raku-shaped pieces slot into that picture, and which of my usual configuration patterns transfered cleanly.

My stack is [**Cro**](https://raku.land/zef:cro) for HTTP, [**Template::HAML**](https://raku.land/zef:gdonald/Template::HAML) for views, **Apache** in front of it for TLS and static assets, and **systemd** to keep the process alive.

**Why not just use a binary distribution of Raku?**

There's a perfectly good [rakudo-pkg](https://github.com/nxadm/rakudo-pkg) project that ships precompiled Debian packages, and [rakubrew](https://rakubrew.org/) for managing multiple Rakudo installs across versions. I looked at both. But I wanted to track HEAD of [MoarVM](https://github.com/MoarVM/MoarVM), [NQP](https://github.com/Raku/nqp), and [Rakudo](https://github.com/rakudo/rakudo) so I could file good bug reports against the bleeding edge if anything misbehaved in production, and both rakudo-pkg and rakubrew's prebuilt releases lag the upstream repos a bit. So I went with a source build on the server, the same way I do it locally. I already had an `install_raku.sh` in my `~/bin` for local installs. I ran that on my server.

**SIGKILL, exit code 137, and what my VPS was actually telling me**

The build died about eight minutes in:

```bash
compiling src/strings/unicode.o
... [snip several hundred MoarVM compilation lines] ...
Stage parse      : 142.928
Stage syntaxcheck:   0.000
Stage ast        :   0.000
Stage optimize   : 104.482
Stage mast       : Killed
make: *** [Makefile:1378: blib/CORE.c.setting.moarvm] Error 137
```

The `CORE.c.setting.moarvm` compile is a huge memory hog I've found. It runs one raku process that needs roughly 2.5+GB of resident memory. As far as I know, it cannot be parallelized away and it cannot be made smaller.

[Exit code](https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html) 137 means the process was killed by signal 9, which is `SIGKILL`. When a process exits because of a signal, the shell reports its exit code as 128 plus the signal number, so 9 becomes 137 and 137 means out of memory (OOM). Nothing in userspace sends `SIGKILL` casually. That's the Linux Kernel's OOM killer reaching in and ending a process because the system ran out of memory.

```bash
$ free -h
               total        used        free      shared  buff/cache   available
Mem:           1.9Gi       309Mi       1.5Gi       8.0Mi       152Mi       1.5Gi
Swap:          1.0Gi       739Mi       284Mi
```

1.9 GB of RAM plus 1 GB of swap is 2.9 GB total, and 739 MB of the swap was already in use from other processes before the build started. The mast stage's working set wouldn't fit.

I fixed it by growing the swap to 4 GB with the usual Linux commands: `swapoff /swap`, `rm /swap`, `fallocate -l 4G /swap`, `mkswap`, `swapon`. Since I was already using a file-backed swap mounted via fstab, no fstab change was needed.

While I was poking around with `df -h` I also noticed I had thirteen `/snap/loop*` mounts. Snap keeps three revisions of each installed snap by default. On this box that meant duplicate copies of `core`, `core20`, `core24`, `emacs`, `btop`, and `certbot` all sitting there, eating roughly 2 GB of disk between them. Two commands cleared half of those out:

```bash
snap set system refresh.retain=2

snap list --all | awk '/disabled/{print $1, $3}' | \
  while read name rev; do snap remove "$name" --revision="$rev"; done
```

The first line drops snapd's retention from three revisions to two so the duplicates won't pile up so much again. The awk loop walks `snap list --all`, pulls the name plus revision number of each row marked `disabled`, and removes them one at a time. Tangential to the Raku build, but useful housekeeping while I was already in there. Sidequests are fun!

Round two of `./install_raku.sh` took 15 minutes, most of that the mast stage paging into the new swap, and ended with a working `raku -v`.

```bash
> raku -v
Welcome to Rakudo™ v2026.05-32-g9c322706e.
Implementing the Raku® Programming Language v6.d.
Built on MoarVM version 2026.05-3-g236fcdf7c.
```

**Capistrano, but for Raku**

I thought about it but eventually didn't actually use [Capistrano](https://capistranorb.com/) for deploying. Under the hood it's really just a nice way to run a shell script. But I did follow a layout like it provides:

```bash
/raku/behave.dev/
├── current -> releases/20260525155724    # atomic symlink to active release
├── releases/                             # one timestamped directory per deploy
├── repo/                                 # bare git mirror
├── revisions.log                         # append-only deploy log
└── shared/                               # cross-release files (uploaded media, etc)
```

Every deploy is a fresh checkout into a new release directory. The `current` symlink flips atomically with `rename(2)`, and rollback is one `ln -sfn` away. I wanted the same shape for my Raku app.

The pieces I needed to reproduce in shell: a bare git repo on the server (`git clone --bare git@github.com:gdonald/behave.dev.git repo`), a script that runs from my laptop and opens an SSH session to the server, each release dir populated via `git archive | tar -x` from the bare repo (clean checkout, no `.git` directory inside the release), a `zef install --deps-only` for dependencies, a precompile step so the first request after restart isn't slow, the atomic symlink flip, a `systemctl --user restart`, an append to `revisions.log`, and a prune of old releases, keeping the five most recent.

The final `deploy.sh` is about thirty lines of bash. Each step is its own `ssh "$SSH_TARGET" "..."` call, deliberately. I considered wrapping them in a heredoc or a helper function, but either approach felt clever in a way I'd regret in six months. Plain repeated `ssh` invocations are the most boring thing that works, and boring is exactly what I want from a deploy script.

The atomic symlink flip is pretty cool:

```bash
ssh "$SSH_TARGET" "ln -s $RELEASE $CURRENT.new && mv -T $CURRENT.new $CURRENT"
```

`mv -T` from GNU coreutils forces the target to be treated as a file rather than a directory-to-move-into. The actual `rename(2)` syscall is atomic on the same filesystem, which means there is no observable window where `current` points at the old release, then nowhere, then the new release. It is always one or the other.

**The PATH gotcha**

The first time I ran `./deploy.sh` it died with `zef: command not found`. But `which zef` on the server returned a perfectly valid path. I realized non-interactive SSH sessions don't load `~/.bashrc`. My interactive shell had been adding `/home/gd/rakudo/bin` and `/home/gd/rakudo/share/perl6/site/bin` to `$PATH` all along, but `ssh host "cmd"` sees a minimal default `$PATH` with no knowledge of where Rakudo lives.

And the fix isn't just to use the absolute path to `zef` either, because `zef` itself shells out to `raku` internally, and that subprocess inherits the broken `$PATH`. So I prepend the Rakudo binaries to `$PATH` at the start of the relevant remote commands:

```bash
RAKU_PATH="/home/gd/rakudo/bin:/home/gd/rakudo/share/perl6/site/bin"
ssh "$SSH_TARGET" "export PATH=$RAKU_PATH:\$PATH && cd $RELEASE && zef install --deps-only --/test ."
```

The `\$PATH` is escaped on purpose: `$RAKU_PATH` expands locally (so I can edit it once at the top of the file), but `\$PATH` expands on the server (so the existing remote `$PATH` is preserved). This kind of two-level quoting is one of the reasons I distrust clever shell stuff.

**Precompiling at deploy time**

One line in the deploy script:

```bash
raku -I "$RELEASE/lib" -c "$RELEASE/service.raku"
```

The `-c` flag means "compile only, don't execute." As a side effect of compilation, Rakudo writes bytecode into `.precomp/` directories for `service.raku` and every module it pulls in via `use`: Cro::HTTP, Template::HAML, my own `Behave::Web::Routes`, the lot. Without this step, the first HTTP request after a restart would trigger that compilation lazily, in the request path, and the user would sit there for more than a few seconds, waiting. That cost has to happen somewhere. I'd rather pay it during deploy than make a real visitor wait for it.

**systemd, user units, and the linger gotcha**

I run the Cro process under a user systemd unit, installed at `~/.config/systemd/user/behave.dev.service`, with `ExecStart` pointed at `raku`:

```bash
[Unit]
Description=behave.dev Raku App
After=network.target

[Service]
Type=simple
Environment=BEHAVE_DEV_HOST=127.0.0.1
Environment=BEHAVE_DEV_PORT=10000
WorkingDirectory=/raku/behave.dev/current
ExecStart=/home/gd/rakudo/bin/raku -I /raku/behave.dev/current/lib /raku/behave.dev/current/service.raku
Restart=always
KillMode=process
RestartSec=5

[Install]
WantedBy=default.target
```

User units have one critical trap: by default, systemd tears down a user's manager the moment that user's last session logs out. So `systemctl --user enable --now behave.dev` happily starts the service, and then the service vanishes the moment the SSH connection closes. The fix is one command:

```bash
$ loginctl enable-linger gd
```

[Lingering](https://www.freedesktop.org/software/systemd/man/loginctl.html) tells systemd to spin up that user's manager at boot and keep it running until shutdown, independent of any login session. Combined with the unit being `enable`d, the service now starts at boot and survives forever. My first run-in with linger years ago cost me a lot of Googling to figure out why the site died the moment I exited SSH.

The deploy script's `systemctl --user restart behave.dev` needs one more thing to work over a non-interactive SSH connection: `XDG_RUNTIME_DIR` isn't set automatically, and without it `systemctl --user` can't find the user manager's D-Bus socket. So my deploy script sets it inline:

```bash
ssh "$SSH_TARGET" "XDG_RUNTIME_DIR=/run/user/\$(id -u) systemctl --user restart behave.dev"
```

That escaped `\$(id -u)` evaluates on the server, where it returns gd's UID. With that in place, the deploy can restart the service the same way a logged-in shell would.

**Apache, certbot, and a redirect loop**

I have Apache proxying Cro for two reasons: TLS termination (so Cro doesn't have to manage certs), and serving static assets directly (so my `/assets/*` requests don't make a full trip through the Raku process). The vhost is conventional: `Alias` for `/assets/`, `ProxyPass` for everything else, with `ProxyPass /assets/ !` short-circuiting the proxy so Apache handles those itself:

```bash
Alias /assets/ /raku/behave.dev/current/assets/
<Directory /raku/behave.dev/current/assets/>
  Require all granted
</Directory>

ProxyPass /assets/ !
ProxyPass        / http://localhost:10000/
ProxyPassReverse / http://localhost:10000/
```

The order matters: `ProxyPass /assets/ !` must come before `ProxyPass /`, or Apache evaluates the catch-all first and proxies asset requests to Cro anyway.

Setting up certbot is a two-pass process. Certbot's `--apache` plugin needs a working HTTP-only vhost to complete the ACME HTTP-01 challenge. So I create a minimal `:80` vhost with just `ServerName` and a `DocumentRoot`, reload Apache, run `certbot --apache -d behave.dev`, then wire up the `:443` vhost with the proxy directives.

I ran into a redirect loop on first reload. Every request to `https://behave.dev/` returned a 301 pointing at `https://behave.dev/`. It turned out I had two `:443` vhosts both claiming the same `ServerName`: one hand-crafted with `ProxyPass`, and another left behind by certbot's initial pass that contained a leftover `Redirect permanent / https://behave.dev/`. Apache picked the certbot-generated one first because it sorted earlier in `sites-enabled`. Running `a2dissite` on the leftover vhost fixed it.

The other gotcha was the `www.` subdomain. My initial cert was issued for `behave.dev` only, so `https://www.behave.dev/` threw cert-validation errors in browsers. `certbot --apache -d behave.dev -d www.behave.dev --expand` reissued a SAN cert covering both names under the same filename, and the existing vhost paths kept working.

I'm a software engineer by day, but I do enjoy occasional sysadmin/devops work.
