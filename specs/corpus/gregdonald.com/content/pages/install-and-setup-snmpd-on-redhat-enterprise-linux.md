---
title: Install and setup snmpd on RedHat Enterprise Linux
date: 2020-06-06
slug: install-and-setup-snmpd-on-redhat-enterprise-linux
description: I wanted to access a RHEL machine from Cacti , so I needed to install and configure net-snmp on there first. yum install net-snmp After installing net-snmp the snmpd.conf needs to be configured: vi...
tags: [cacti, rhel, snmpd]
archives: [2020-06]
---
I wanted to access a RHEL machine from [Cacti](https://www.cacti.net/), so I needed to install and configure net-snmp on there first.

```bash
yum install net-snmp
```

After installing net-snmp the snmpd.conf needs to be configured:

```bash
vi /etc/snmp/snmpd.conf
```

Adding the following enables access from other machines on my local network:

```bash
view   systemonly  included   .1.3.6.1.2.1.1
view   systemonly  included   .1.3.6.1.2.1.25.1

rocommunity public 12.34.56.78/32

rocommunity  public default -V systemonly
rocommunity6 public default -V systemonly
```

Next I setup the snmpd service to start on boot:

```bash
systemctl enable snmpd.service
systemctl restart snmpd.service
```
