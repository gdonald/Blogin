---
title: Database tasks
slug: db-tasks
order: 6
description: The active-record db:* tasks manage the database lifecycle: creating and dropping it, running and inspecting migrations, and seeding. They act on the...
toc: true
---
The `active-record db:*` tasks manage the database lifecycle: creating and dropping it,
running and inspecting migrations, and seeding. They act on the active
environment's configured connections (all of them for a multi-database setup);
migration-state tasks report on the primary connection.

## Lifecycle

```
active-record db:create     # create the configured database(s)
active-record db:drop       # drop the configured database(s)
active-record db:setup      # create + migrate + seed
active-record db:reset      # drop + create + migrate + seed
active-record db:prepare    # create, migrate, and seed only when the database is new
```

`db:create` and `db:drop` operate on the database itself (the file for SQLite,
the `CREATE DATABASE` / `DROP DATABASE` for PostgreSQL and MySQL). `db:setup`
and `db:reset` are the full rebuild paths; `db:prepare` is the idempotent one
that seeds a freshly created database but leaves an existing one untouched.

## Migrating

```
active-record db:migrate                 # apply every pending migration
active-record db:migrate VERSION=20260615120000
active-record db:migrate:up VERSION=...   # run one migration's up
active-record db:migrate:down VERSION=... # run one migration's down
active-record db:rollback                 # roll back the last migration
active-record db:rollback STEP=3          # roll back the last 3
active-record db:migrate:redo             # roll back then re-apply (STEP=N supported)
```

`db:migrate VERSION=NNN` brings the schema to exactly that version: it applies
every pending migration at or below the target and rolls back every applied
migration above it. `VERSION=0` rolls everything back.

## Inspecting

```
active-record db:version                       # the highest applied version
active-record db:migrate:status                # each migration as up / down
active-record db:check                         # every database exists and is migrated
active-record db:abort_if_pending_migrations   # exit non-zero if any migration is pending
```

`db:check` verifies, without changing anything, that every database the active
environment expects exists and has all its migrations applied. It exits non-zero
and prints one line per problem when something is missing or behind, so it works
as a pre-flight before a deploy or a test run.

`db:migrate:status` prints one line per migration:

```
up    20260101120000  create-users
up    20260102090000  add-email-to-users
down  20260615120000  create-widgets
```

`db:abort_if_pending_migrations` is the guard to put in front of a deploy or a
test run: it exits `1` (and lists the pending versions) when the schema is
behind, and `0` otherwise.

## Seeding

```
active-record db:seed
```

Runs `db/seeds.raku`, an ordinary Raku script that uses the ORM to insert
records. It runs in the CLI process against the active connection, so it loads
its own models:

```perl6
use lib 'app/models';
use Models::User;

Models::User.create({ fname => 'Ada',   lname => 'Lovelace' });
Models::User.create({ fname => 'Alan',  lname => 'Turing' });
```

`db:setup`, `db:reset`, and a new-database `db:prepare` all run the seeds as
their last step.

## Test database

```
active-record db:test:prepare        # create + migrate the test database
active-record db:test:load_schema    # alias of db:test:prepare
```

These ready the `test` environment's database. For the parallel test setup (the
per-worker databases behave uses), `db:create`, `db:migrate`, `db:check`, and
`db:reset` accept `--parallel`, which targets every worker copy instead of the
single base database. The worker count comes from the test environment's
`parallel` key in `config/application.json` (or an explicit `--parallel=N`):

```
active-record db:create  --parallel        # create the N per-worker copies
active-record db:migrate --parallel        # migrate them
active-record db:check   --parallel        # verify all N are ready
active-record db:reset   --parallel --yes  # drop every worker table
```

A parallel `db:reset` drops every worker table and is non-interactive, so it
requires `--yes` (or `AR_ASSUME_YES=1`).

## Schema dump and load

```
active-record db:schema:dump     # introspect the database into db/schema.raku
active-record db:schema:load     # purge, then rebuild the database from db/schema.raku
active-record db:structure:dump  # dump the raw DDL to db/structure.sql
```

`db/schema.raku` is a `Migration` subclass that recreates the tables with the
migration DSL, plus the list of applied versions. Foreign keys are introspected
and reproduced: on PostgreSQL and MySQL as `add-foreign-key` statements after
every table is created, and on SQLite as inline `references` adverbs on the
column (SQLite cannot add a foreign key to an existing table). The dump remains
lossy for column limits and defaults, which are not recoverable through the
introspection API and are omitted. `db:schema:load` drops every table and
re-runs the dumped `up`, then records the schema's versions so the app sees the
migrations as applied (the fast alternative to replaying every migration).

`db:structure:dump` writes the exact, database-specific DDL: the
`sqlite_master` statements on SQLite, and `pg_dump --schema-only` /
`mysqldump --no-data` on PostgreSQL and MySQL. Use it when the schema relies on
features the portable dump cannot represent.

## Schema cache

```
active-record db:schema:cache:dump   # write db/schema_cache.yml
active-record db:schema:cache:clear  # remove db/schema_cache.yml
```

The schema cache is a YAML snapshot of every table's columns, indexes,
constraints, and sequences, so an app can skip live introspection at boot. Load
it with `SchemaCache.load-yaml(:path)`.
