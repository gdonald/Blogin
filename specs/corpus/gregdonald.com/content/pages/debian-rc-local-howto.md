---
title: Debian rc.local howto
date: 2010-01-26
slug: debian-rc-local-howto
description: If you're using a flavor of *nix that has an rc.local file, and then you start using Debian GNU/Linux, you might be wondering where your rc.local file is. Quite simply, it's not there. Here's how to...
tags: [debian, linux]
archives: [2010-01]
---
If you're using a flavor of *nix that has an rc.local file, and then you start using Debian GNU/Linux, you might be wondering where your rc.local file is. Quite simply, it's not there. Here's how to add it.

Create a new file named /etc/init.d/local like this:

```bash
#!/bin/sh
# put startup stuff here
```

Make the file executable:

```bash
chmod 755 /etc/init.d/local
```

Add it to startup:

```bash
update-rc.d local defaults 80
```

You should be seeing something like this:

```bash
Adding system startup for /etc/init.d/local ...
/etc/rc0.d/K80local -> ../init.d/local
/etc/rc1.d/K80local -> ../init.d/local
/etc/rc6.d/K80local -> ../init.d/local
/etc/rc2.d/S80local -> ../init.d/local
/etc/rc3.d/S80local -> ../init.d/local
/etc/rc4.d/S80local -> ../init.d/local
/etc/rc5.d/S80local -> ../init.d/local
```
