---
title: Debian Games
date: 2006-01-14
slug: debian-games
description: Here's a command to install lots of games on your Debian box: yes | \ for x in `apt-cache search game \ | sort \ | awk 'BEGIN { FS = " - " } { print $1 }'`; do \ apt-get install $x; \ done
tags: [debian, game]
archives: [2006-01]
---
Here's a command to install lots of games on your Debian box:

```bash
yes | \
  for x in `apt-cache search game \
      | sort \
      | awk 'BEGIN { FS = " - " } { print $1 }'`; do \
    apt-get install $x; \
  done
```
