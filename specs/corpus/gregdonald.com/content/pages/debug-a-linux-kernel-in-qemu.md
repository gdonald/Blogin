---
title: Debug a Linux Kernel in QEMU
date: 2021-09-15
slug: debug-a-linux-kernel-in-qemu
description: Debug a Linux Kernel in QEMU
tags: [kernel, linux, qemu]
archives: [2021-09]
---
```bash
mkinitramfs -o ../ramdisk.img 5.11.0-34-generic
``` ```bash
qemu-system-x86_64 -kernel arch/x86_64/boot/bzImage \
    -initrd ../ramdisk.img -m 1024 -s -S \
    -append "console=ttyS0 nokaslr" -nographic
``` ```bash
gdb vmlinux
``` ```bash
target remote:1234
``` ```bash
hb start_kernel
```
