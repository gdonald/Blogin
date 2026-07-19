---
title: ignore latin1 problem via psql
date: 2016-10-20
slug: ignore-latin1-problem-via-psql
description: CREATE DATABASE _ WITH ENCODING 'UTF8' OWNER _ LC_COLLATE 'en_US.UTF-8' LC_CTYPE 'en_US.UTF-8' TEMPLATE template0;
tags: [postgresql, psql]
archives: [2016-10]
---
```sql
CREATE DATABASE _ WITH ENCODING 'UTF8' OWNER _ LC_COLLATE 'en_US.UTF-8' LC_CTYPE 'en_US.UTF-8' TEMPLATE template0;
```

In the case of a missing en_US.UTF-8 locale, modify /etc/locale.gen to add it, run locale-gen to generate it, then restart Postgres.
