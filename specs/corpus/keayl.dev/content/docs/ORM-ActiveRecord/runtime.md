---
title: Runtime tasks
slug: runtime
order: 7
description: The active-record runtime subcommands run code against the app, open a database client, and report on the source tree.
toc: true
---
The `active-record` runtime subcommands run code against the app, open a database client,
and report on the source tree.

## active-record console

```
active-record console
```

Starts a Raku REPL with the project's `lib`, `app/models`, and `app/validators`
directories on the include path (the ones that exist). `use` the ORM and your
models to poke at data:

```perl6
> use ORM::ActiveRecord::Model;
> use Models::User;
> Models::User.count;
```

## active-record runner

```
active-record runner script.raku
active-record runner "Models::User.where({ active => True }).count.say"
```

Runs a script file or inline code. The first argument is treated as a file when
it exists on disk, otherwise as code to evaluate. The database connection is
resolved lazily from config, so models work without any setup. Inline code does
not auto-print, so call `.say` (or `say ...`) when you want output.

## active-record dbconsole

```
active-record dbconsole
```

Opens the native client for the active environment's primary connection with
the configured credentials: `sqlite3` for SQLite, `psql` for PostgreSQL (the
password is passed via `PGPASSWORD`), and `mysql` for MySQL.

## active-record notes

```
active-record notes
```

Scans the source tree (`lib`, `bin`, `app`, `db`, `t`) for annotation comments
and lists them with file and line:

```
TODO     lib/foo.rakumod:42  revisit this once the cache lands
FIXME    app/models/user.rakumod:18  handle the nil case
```

The recognised tags are `TODO`, `FIXME`, `OPTIMIZE`, `HACK`, and `XXX`.

## active-record stats

```
active-record stats
```

Counts source files, total lines, code lines (non-blank, non-comment), model
declarations, and migrations:

```
Files:      85
Lines:      14553
Code lines: 11814
Models:     12
Migrations: 57
```
