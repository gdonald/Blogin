---
title: Change default SSH port on Fedora
date: 2019-03-24
slug: change-default-ssh-port-on-fedora
description: I wanted to change my default SSH port to something else. Here are the commands I used.
tags: [fedora, ssh]
archives: [2019-03]
---
I wanted to change my default SSH port to something else. Here are the commands I used:

1) I modified the "Port" value in my SSH config:

```bash
vi /etc/ssh/sshd_config
```

It was commented out. I uncommented it and modified it.

2) I installed "policycoreutils-python-utils" to gain access to the "[semanage](https://linux.die.net/man/8/semanage)" command:

```bash
yum install policycoreutils-python-utils
```

3) I ran "semanage" to add my preferred SSH port to the allowed SSH ports:

```bash
semanage port -a -t ssh_port_t -p tcp xxxx
```

4) I checked to make sure it succeeded:

```bash
semanage port -l | grep ssh_port_t
```

5) I added a firewall exception for my preferred SSH port:

```bash
firewall-cmd --permanent --service="ssh" --add-port "xxxx/tcp"
```

6) I reloaded my firewall and SSH:

```bash
firewall-cmd --reload
systemctl reload sshd
```

7) I rebooted to make sure it all still worked correctly on boot:

```bash
reboot
```
