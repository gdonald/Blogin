---
title: Install and setup irssi and oidentd on Debian
date: 2020-06-01
slug: install-and-setup-irssi-and-oidentd-on-debian
description: Install irssi and oidentd: # apt install oidentd irssi irssi-scripts build-essential See that oidentd is now running: # ps aux | grep oidentd
tags: [debian, irssi, liberachat, oidentd]
archives: [2020-06]
---
Install irssi and oidentd:

```bash
# apt install oidentd irssi irssi-scripts build-essential
```

See that oidentd is now running:

```bash
# ps aux | grep oidentd
```

You should see output like:

```bash
oident 5107 0.0 0.0 8772 160 ? Ss 16:50 0:00 /usr/sbin/oidentd -mf -P 12.34.56.78 -u oident -g oident
```

Modify /etc/oidentd_masq.conf and add your ident response for IRC:

```bash
12.34.56.79 gd UNIX
```

Restart odientd:

```bash
# service oidentd restart
```

-->

Run irssi:

```bash
$ irssi
```

Inside irssi, install [script(s)](https://scripts.irssi.org/):

```bash
/run scriptassist
/script install nickcolor.pl
/script autorun nickcolor.pl
```

Authenticate automatically on startup using SASL:

```bash
/network add -sasl_username foo -sasl_password changeme -sasl_mechanism PLAIN LiberaChat
```

Setup irssi to connect on startup:

```bash
/server add -auto -network LiberaChat irc.libera.chat
```

Setup irssi to join channels on startup:

```bash
/channel add -auto ##linux LiberaChat
/channel add -auto #freebsd LiberaChat
/channel add -auto #openbsd LiberaChat
/channel add -auto ##c LiberaChat
/channel add -auto #perl LiberaChat
/channel add -auto #raku LiberaChat
/channel add -auto #ruby LiberaChat
```

Ignore joins, parts, quits, and nick updates:

```bash
/ignore * join part quit nick
```

Save settings:

```bash
/save
```
