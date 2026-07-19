---
title: Generate new Factorio map
date: 2021-01-17
slug: generate-new-factorio-map
description: su - service factorio stop cd /factorio bin/x64/factorio --create saves/my-save.zip \ --map-gen-settings data/map-gen-settings.json \ --map-settings data/map-settings.json chown -R factorio:factorio...
tags: [factorio, linux]
archives: [2021-01]
---
```bash
su -

service factorio stop

cd /factorio

bin/x64/factorio --create saves/my-save.zip \
                 --map-gen-settings data/map-gen-settings.json \
                 --map-settings data/map-settings.json
                 
chown -R factorio:factorio /factorio

service factorio start
```
