---
title: PostgreSQL Backup Script
date: 2021-03-28
slug: postgresql-backup-script
description: #!/bin/sh DB_NAME=mydatabase DATE=`date +%Y%m%d%H%M%S` DIR=/backups/pgsql PG_DUMP=/usr/bin/pg_dump $PG_DUMP $DB_NAME | bzip2 > $DIR/$DB_NAME_$DATE.sql.bz2 find $DIR -mtime +7 -exec rm {} \;
tags: [backup, database, postgresql]
archives: [2021-03]
---
```bash
#!/bin/sh

DB_NAME=mydatabase
DATE=`date +%Y%m%d%H%M%S`
DIR=/backups/pgsql
PG_DUMP=/usr/bin/pg_dump

$PG_DUMP $DB_NAME | bzip2 > $DIR/$DB_NAME_$DATE.sql.bz2

find $DIR -mtime +7 -exec rm {} \;
```

This script is a simple shell script for automating the backup of a PostgreSQL database. It's written to be run in a Unix-like environment (such as Linux or macOS). Let me break it down for you:

`#!/bin/sh`: This is the shebang line. It tells the system that this script should be run with `/bin/sh`, which is the standard command interpreter for shell scripts on Unix-like systems.

`DATE=$(date +%Y%m%d%H%M%S)`: This line sets a variable named `DATE`. The `date` command is used to generate a timestamp in the format of YearMonthDayHourMinuteSecond (e.g., 20240114235959 for January 14, 2024, at 23:59:59). This timestamp is used to uniquely name each backup file.

`DIR=/backups/pgsql`: Sets the `DIR` variable to the directory where the backups will be stored, `/backups/pgsql` in this case.

`PG_DUMP=/usr/bin/pg_dump`: This line sets the `PG_DUMP` variable to the path of the `pg_dump` utility. `pg_dump` is a utility provided by PostgreSQL to dump (backup) the contents of a database.

`$PG_DUMP mydatabase | bzip2 > $DIR/mydatabase_$DATE.sql.bz2`: This line is the core of the script. It uses `pg_dump` to dump the database named `mydatabase`. The output of `pg_dump` is piped (`|`) into `bzip2`, which compresses the data. The compressed backup is then redirected (`>`) to a file in the specified directory with the database name, the date, and the `.sql.bz2` extension. For example, the backup file could be named `mydatabase_20240114235959.sql.bz2`.

`find $DIR -mtime +7 -exec rm {} \;`: This line cleans up old backups. It uses the `find` command to look for files in the backup directory (`$DIR`) that were modified more than 7 days ago (`-mtime +7`). For each of these files, the `rm` command is executed (`-exec rm {} \;`) to delete the file. This ensures that the backup directory doesn't get filled up with old backups.

This small shell script provides a simple way to regularly backup a PostgreSQL database and clean up old backups to conserve storage space. The script could be scheduled to run at regular intervals using a cron job or a similar scheduling utility.
