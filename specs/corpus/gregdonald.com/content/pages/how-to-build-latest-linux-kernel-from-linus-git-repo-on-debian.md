---
title: How-to build latest Linux kernel from Linus' git repo on Debian
date: 2017-01-01
slug: how-to-build-latest-linux-kernel-from-linus-git-repo-on-debian
description: Here's a how-to for building a recent Linux kernel on your Debian GNU/Linux box: You will need to do all this as root. It's serious business building new Linux kernels :) su - The dash after the su...
tags: [debian, kernel, linux, ubuntu]
archives: [2017-01]
---
Here's a how-to for building a recent Linux kernel on your Debian GNU/Linux box:

You will need to do all this as root. It's serious business building new Linux kernels :)

```bash
su -
```

The dash after the su command makes it behave as if you had logged in as root directly, a full login environment is applied.

Make sure you have the required tools and libraries installed:

```bash
apt install build-essential initramfs-tools procps libncurses5-dev \
    fakeroot git-core screen zlib1g-dev flex bison bc libelf-dev:native \
    libssl-dev:native dwarves rsync debhelper
```

Use git to clone Linus' latest git repo:

```bash
cd /usr/src

git clone git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
```

You may alternately want to run the latest stable kernel, which would be:

```bash
git clone git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
```

Either way the clone will take a bit:

```bash
Cloning into 'linux'...
remote: Counting objects: 2725713, done.
remote: Compressing objects: 100% (412816/412816), done.
remote: Total 2725713 (delta 2286272), reused 2725359 (delta 2285962)
Receiving objects: 100% (2725713/2725713), 559.28 MiB | 3.30 MiB/s, done.
Resolving deltas: 100% (2286272/2286272), done.
```

Once you have the source you can checkout a specific branch. To see all the remote branches/tags:

```bash
cd linux

git tag | sort -V
```

As of today the latest stable is v5.11.0 so you can use this command to get that version:

```bash
git checkout tags/v5.11.0 -b v5.11.0
```

You should see:

```bash
Switched to a new branch 'v5.11.0'
```

You can also see what branches and tags are available here:

[http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/refs/](http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/refs/)

or here: [https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git)

Now you're ready for configuration. I base my new kernel configuration on a known working configuration, then trim it down from there. Check to see what configurations you have in /boot:

```bash
ls /boot/config*
```

Configure your new kernel source using your chosen config file:

```bash
make menuconfig
```

Select "Load an Alternate Configuration File", enter your config file path, for example I used /boot/config-4.4.0-57-generic. Hit exit and save.

You might also consider using localmodconfig:

```bash
make localmodconfig
```

This command will attempt to automatically configure support for your specific hardware. Just make sure all your hardware devices you want supported are plugged in before running the command.

Build the kernel and package it:

```bash
screen make -j`nproc --all` bindeb-pkg
```

screen is a command used to run another command in a virtual screen. The new virtual screen doesn't end if you disconnect. `man screen` if you're not familiar, it's a very useful tool.

Build a new kernel using a distro's (Debian in my case) default config takes a while. Everything will usually work on the first try using a distro config since everything is built as modules as much as possible, and all modules get built. You stand a good chance of successfully booting a new kernel built this way. Later you can remove stuff from the config and rebuild. Wash, rinse, and repeat until you get your kernel config down to just the hardware you actually have in your system.

Install the new kernel:

```bash
cd ..

dpkg -i linux-image-5.11.0-rc7_5.11.0-rc7-1_amd64.deb
```

If you need headers, you can install that now too:

```bash
dpkg -i linux-headers-5.11.0-rc7_5.11.0-rc7-1_amd64.deb
```

Reboot.

When your system comes back up..

```bash
> uname -a
Linux jupiter 5.11.0-rc7 #1 SMP Fri Feb 26 09:06:55 CDT 2021 x86_64 x86_64 x86_64 GNU/Linux
```

Side note, on Ubuntu I get:

```bash
*** No rule to make target 'debian/canonical-revoked-certs.pem', needed by 'certs/x509_revocation_list'.  Stop.
```

I found I could disable revocation keys using:

```bash
scripts/config --disable SYSTEM_REVOCATION_KEYS
```
