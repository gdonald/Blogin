---
title: Cisco CCNA Study :: Home Lab Notes
date: 2020-12-20
slug: cisco-ccna-study-home-lab-notes
description: #################################################################### # R1 # Cisco IOS Software, C2900 Software (C2900-UNIVERSALK9-M), Version 15.7(3)M5, RELEASE SOFTWARE (fc1) # Factory reset reload...
tags: [ccna, cisco, home, lab]
archives: [2020-12]
---
```bash
####################################################################

# R1
# Cisco IOS Software, C2900 Software (C2900-UNIVERSALK9-M), Version 15.7(3)M5, RELEASE SOFTWARE (fc1)

# Factory reset

reload

# In minicom, hit: Ctrl A, F

confreg 0x2142  # skip loading startup-config
reset

Would you like to enter the initial configuration dialog? [yes/no]: no

en
write erase

conf t
config-register 0x2102  # re-enable loading startup-config
end

write

reload

# set host and domain

conf t
host R1
ip domain-name localdomain
end
write

# configure g0/0

conf t
interface GigabitEthernet0/0
ip address 12.34.56.78 255.255.255.0
no shut
end
write

# add user

conf t
service password-encryption
enable secret changeme
username username password changeme

# configure SSH

crypto key generate rsa

line vty 0 4
transport input ssh
login local
password 0 changeme
exit

line console 0
logging synchronous
login local

# `ssh -oKexAlgorithms=+diffie-hellman-group-exchange-sha1,diffie-hellman-group1-sha1 -c aes256-ctr username@12.34.56.78`

####################################################################

Cisco ASA 5510 - Version 8.2(5)

interface Ethernet0/0
nameif outside
security-level 0
ip address 12.34.56.78 255.255.255.0 

interface Ethernet0/1
nameif inside
security-level 100
ip address 12.34.254 255.255.255.0

# NAT

nat (inside) 1 0.0.0.0 0.0.0.0
global (outside) 1 interface

# on inside subnet, `apt update` now works on host 12.34.0.10

# add access for inside web server 12.34.0.10

static (inside,outside) tcp interface www 12.34.0.10 www netmask 255.255.255.255
access-list outside extended permit tcp any any eq www

static (inside,outside) tcp interface https 12.34.0.10 https netmask 255.255.255.255
access-list outside extended permit tcp any any eq https

access-group outside in interface outside

# from a host on 12.34.56.0/24 `curl 12.34.56.78` now works

# allow echo-reply, so the reply to a ping can reach the inside

access-list outside extended permit icmp any any echo-reply
# access-group outside in interface outside   # already done above, doing it again clears previous :(

# allow traceroute from inside

access-list outside extended permit icmp any any time-exceeded  # type 11
access-list outside extended permit icmp any any unreachable    # type 3

# allow SSH

enable password changeme
username username password changeme privilege 15
aaa authentication ssh console LOCAL
ssh 12.34.56.78 255.255.255.0 outside

domain-name localdomain
crypto key generate rsa mod 2048
ssh version 2

# from outside `ssh -oKexAlgorithms=+diffie-hellman-group1-sha1 -c aes128-cbc username@12.34.56.78`

####################################################################

Configure telnet access:

username gd secret changeme
enable secret changeme

line vty 0 4
logging synchronous
login local
transport input telnet

####################################################################

# cisco 1841 factory reset

# get to the `rommon 1>` prompt

# In minicom hit Ctrl a, f

# or maybe using Ctrl + Break or (see below)

rommon 1 > confreg 0x2142

# or for ASA:

confreg 0x41

rommon 2 > reset

[...]

Would you like to enter the initial configuration dialog? [yes/no]: no

Router>en

Router#write erase

Router#conf t

Router(config)#config-register 0x2102

Router(config)#^Z

From https://dcloud-cms.cisco.com/help/reset_router

####################################################################

# serial over USB breaks Ctrl + b

# as root
sudo su -

# simulate sending Ctrl + b using screen
screen -L /dev/ttyUSB0 1200

# then hold space for 15 seconds

# then kill screen

Ctrl + a, d
killall -9 screen

# and then back in minicom
minicom -s

# there will be the `rommon>` prompt

####################################################################

# Cisco 3550 factory reset:

# power up while holding "mode" button, then:

flash_init
delete flash:config.text
delete flash:vlan.dat
boot
```
