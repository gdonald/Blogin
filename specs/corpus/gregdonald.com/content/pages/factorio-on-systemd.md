---
title: Factorio on systemd
date: 2021-01-17
slug: factorio-on-systemd
description: # adduser factorio # systemctl enable factorio.service # service factorio start # /etc/systemd/system/factorio.service [Unit] Description=Factorio Server [Service] Type=simple User=factorio...
tags: [factorio, linux, systemd]
archives: [2021-01]
---
```ini
# adduser factorio
# systemctl enable factorio.service
# service factorio start

# /etc/systemd/system/factorio.service
[Unit]
Description=Factorio Server

[Service]
Type=simple
User=factorio
ExecStart=/factorio/bin/x64/factorio --start-server-load-latest --server-settings /factorio/data/server-settings.json
WorkingDirectory=/factorio
Restart=on-failure
KillSignal=SIGINT

[Install]
WantedBy=multi-user.target
```
