---
title: Install xfce4 on Debian
date: 2020-05-31
slug: install-xfce4-on-debian
description: This is how I install xfce4 on a new Debian.
tags: [apt, awk, debian, xargs, xfce4]
archives: [2020-05]
---
This is how I install xfce4 on a new Debian:

```bash
yes | \
  for x in `apt-cache search xfce4 \
      | sort \
      | awk 'BEGIN { FS = " - " } { print $1 }'`; do \
    apt install $x; \
  done
```

Or if you prefer a less transactional install, you can use xargs

```bash
apt-cache search xfce4 \
  | sort \
  | awk 'BEGIN { FS = " - " } { print $1 }' \
  | xargs apt install -y
```
