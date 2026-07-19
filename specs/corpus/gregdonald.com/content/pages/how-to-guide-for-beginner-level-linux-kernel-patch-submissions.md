---
title: How-to guide for beginner-level Linux Kernel patch submissions
date: 2015-06-20
slug: how-to-guide-for-beginner-level-linux-kernel-patch-submissions
description: Every day I see new Linux Kernel hackers fail at their first patch submission. I'm not an expert, but I've learned how the process works and most importantly I've learned how to avoid irritating...
tags: [diff, git, kernel, linux, patch, sed]
archives: [2015-06]
---
Every day I see new Linux Kernel hackers fail at their first patch submission. I'm not an expert, but I've learned how the process works and most importantly I've learned how to avoid irritating Linux Kernel maintainers. The "maintainers" are the gate keepers to the Linux Kernel. If you piss them off you will never land any patches into the Linux Kernel.

All Linux Kernel development takes place in the open and hundreds (thousands?) of Linux Kernel developers will see and possibly read your patch submissions. You will want to make every effort to submit the best possible patch you can. That's where I come in. If you follow my guide there's a better than average chance you will actually land your patch into the Linux Kernel.

For a beginner I recommend working on the drivers/staging tree maintained by Greg Kroah-Hartman. Clone Greg KH's staging tree:

```bash
 git clone git://git.kernel.org/pub/scm/linux/kernel/git/gregkh/staging.git
```

This will take a while. After that you need to checkout the staging-testing branch:

```bash
> git checkout staging-testing
```

This is the branch you should base your new work on.

Next you need to find something to fix using the checkpatch.pl script found in the scripts directory. checkpatch.pl when used with the -f flag may produce errors or warnings when ran against .c or .h files. Once you find an error or warning you can make a patch to fix it. Here's an example run:

```bash
> scripts/checkpatch.pl -f drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c
```

This produces lots of errors and warnings. One of them is:

```clike
ERROR: space required after that ',' (ctx:VxV)
#350: FILE: drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c:350:
+	tag = (u8 *) skb_put(skb,len+2+rate_len);
 	                        ^
```

To fix this error you first need to make a new git branch based on the staging-testing branch:

```bash
> git checkout staging-testing
> git checkout -b 2015-06-19-drivers-staging-space-required-after-that-comma
```

Linux Kernel development produces a lot of git branches so I've added the date and a short description to my branch name so I'm not confused by other branches still in my tree.

Next I modify the drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c file at line 350 to fix the error.

After the modification I can see my change by running git diff:

```bash
> git diff
diff --git a/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c b/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c
index 1b11acb..2a3694b 100644
--- a/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c
+++ b/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c
@@ -347,7 +347,7 @@ inline struct sk_buff *ieee80211_probe_req(struct ieee80211_device *ieee)
        memcpy(req->header.addr2, ieee->dev->dev_addr, ETH_ALEN);
        memset(req->header.addr3, 0xff, ETH_ALEN);
 
-       tag = (u8 *) skb_put(skb,len+2+rate_len);
+       tag = (u8 *) skb_put(skb, len+2+rate_len);
 
        *tag++ = MFIE_TYPE_SSID;
        *tag++ = len;
```

No matter how small the change it must be tested before submission. To test this change I need to change the configuration of my Linux Kernel so the code that was changed is compiled and then I need to compile it.

First I need to run `make menuconfig` and configure this driver to be compiled as a module. Here are some screenshots:

[![Linux Kernel make menuconfig device drivers](/assets/img/linux-kernel-make-menuconfig-device-drivers.png){width=640}](/assets/img/linux-kernel-make-menuconfig-device-drivers.png){target=_blank rel=noopener}

[![Linux Kernel make menuconfig staging drivers](/assets/img/linux-kernel-make-menuconfig-staging-drivers.png){width=640}](/assets/img/linux-kernel-make-menuconfig-staging-drivers.png){target=_blank rel=noopener}

[![Linux Kernel make menuconfig realtek driver](/assets/img/linux-kernel-make-menuconfig-realtek-driver.png){width=640}](/assets/img/linux-kernel-make-menuconfig-realtek-driver.png){target=_blank rel=noopener}

After that I run make for this particular module and see that it compiles:

```bash
> make M=drivers/staging/rtl8192u
```

There may be other warnings in the output and that's OK, just as long as the change I made doesn't produce a new error. The output looks like this:

```bash
Building modules, stage 2.
MODPOST 1 modules
CC      drivers/staging/rtl8192u/r8192u_usb.mod.o
LD [M]  drivers/staging/rtl8192u/r8192u_usb.ko
```

Next I need to commit my change to my working branch:

```bash
> git commit -a
```

This will open my text editor and prompt me for a commit message. My commit message for this change will be:

```bash
drivers: staging: Fix "space required after that ','" errors

Fix checkpatch.pl "space required after that ','" errors

Signed-off-by: Greg Donald <gdonald@gmail.com>
```

This commit message will later be broken down into two parts when runnnig git send-email. The first line will become the subject of the email and the rest will become part of the body of the email. A "Signed-off-by" is required for every Linux Kernel patch.

After committing my changes I now need to produce a patch that describes my changes. The best way to do that is to use git format-patch and provide parameters comparing the staging-testing branch against my working branch:

```bash
> git format-patch staging-testing..2015-06-19-drivers-staging-space-required-after-that-comma
```

This produces a patch file that is appropriate for use with git send-email:

```bash
> cat 0001-drivers-staging-Fix-space-required-after-that-errors.patch 
From 271b29e14d6cff21dd8aa911ed5374eeadcc3dc6 Mon Sep 17 00:00:00 2001
From: Greg Donald <gdonald@gmail.com>
Date: Fri, 19 Jun 2015 12:55:20 -0500
Subject: [PATCH] drivers: staging: Fix "space required after that ','" errors

Fix checkpatch.pl "space required after that ','" errors

Signed-off-by: Greg Donald <gdonald@gmail.com>
---
 drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)

diff --git a/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c b/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c
index 1b11acb..2a3694b 100644
--- a/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c
+++ b/drivers/staging/rtl8192u/ieee80211/ieee80211_softmac.c
@@ -347,7 +347,7 @@ inline struct sk_buff *ieee80211_probe_req(struct ieee80211_device *ieee)
 	memcpy(req->header.addr2, ieee->dev->dev_addr, ETH_ALEN);
 	memset(req->header.addr3, 0xff, ETH_ALEN);
 
-	tag = (u8 *) skb_put(skb,len+2+rate_len);
+	tag = (u8 *) skb_put(skb, len+2+rate_len);
 
 	*tag++ = MFIE_TYPE_SSID;
 	*tag++ = len;
-- 
1.9.1
```

Now my patch has to be checked to make sure it doesn't introduce any new errors or warnings so I run scripts/checkpatch.pl without the -f flag:

```bash
> scripts/checkpatch.pl 0001-drivers-staging-Fix-space-required-after-that-errors.patch 
total: 0 errors, 0 warnings, 8 lines checked

0001-drivers-staging-Fix-space-required-after-that-errors.patch has no obvious style problems and is ready for submission.
```

If errors or warnings are present then this patch would not be suitable for submission. My patch is OK, so the next step is to email it to the proper maintainers for review. To find out who to email it I run scripts/get_maintainer.pl against the patch file and pipe the output to a few cleanup and formatting commands:

```bash
> scripts/get_maintainer.pl 0001-drivers-staging-Fix-space-required-after-that-errors.patch | sed 's/(.*)//g' | sed 's/[ ]*$//' | sed 's/\(.*\)/"\1"/g' | sed 's/^/--to /g' | tr '\n' ' '
--to "Greg Kroah-Hartman <gregkh@linuxfoundation.org>" --to "Cristina Opriceana <cristina.opriceana@gmail.com>" --to "Gaston Gonzalez <gascoar@gmail.com>" --to "Haneen Mohammed <hamohammed.sa@gmail.com>" --to "Greg Donald <gdonald@gmail.com>" --to "Aya Mahfouz <mahfouz.saif.elyazal@gmail.com>" --to "Paul Gortmaker <paul.gortmaker@windriver.com>" --to "devel@driverdev.osuosl.org" --to "linux-kernel@vger.kernel.org"
```

Using git send-email I email my patch:

```bash
> git send-email 0001-drivers-staging-Fix-space-required-after-that-errors.patch --to "Greg Kroah-Hartman <gregkh@linuxfoundation.org>" --to "Cristina Opriceana <cristina.opriceana@gmail.com>" --to "Gaston Gonzalez <gascoar@gmail.com>" --to "Haneen Mohammed <hamohammed.sa@gmail.com>" --to "Greg Donald <gdonald@gmail.com>" --to "Aya Mahfouz <mahfouz.saif.elyazal@gmail.com>" --to "Paul Gortmaker <paul.gortmaker@windriver.com>" --to "devel@driverdev.osuosl.org" --to "linux-kernel@vger.kernel.org"
```
