---
title: Inspection
slug: inspection
order: 22
description: A small family of methods for asking *about* a relation rather than running it. They live on both Model (where they apply to Model.all) and on a Query...
toc: true
---
A small family of methods for asking *about* a relation rather than running
it. They live on both `Model` (where they apply to `Model.all`) and on a
`Query` relation so they compose with `where`, `order`, etc.

## to-sql

`to-sql` returns the SQL string that the relation would execute, with
adapter-specific placeholders (`$N` for PostgreSQL, `?` for SQLite and
MySQL) left in. Bind values are not inlined.

```perl6
say User.where({active => True}).annotate('debug').to-sql;
```

See [Raw SQL and CTEs](/docs/ORM-ActiveRecord/raw) for the full set of escape hatches.

## explain

`explain` runs `EXPLAIN` against the underlying SELECT and returns the plan
rows joined by newlines. SQLite uses `EXPLAIN QUERY PLAN`; PostgreSQL and
MySQL use plain `EXPLAIN`.

```perl6
say User.where({active => True}).order('lname').explain;
say User.explain;                  # whole-table plan
```

`explain` does not materialize the relation — it only EXPLAINs the SQL it
would run, so it is cheap on large tables.

## Relation predicates

| Method     | Returns                              |
| ---------- | ------------------------------------ |
| `is-any`   | `True` when the relation has ≥ 1 row |
| `is-empty` | `True` when the relation has 0 rows  |
| `is-one`   | `True` when the relation has exactly 1 row |
| `is-many`  | `True` when the relation has > 1 row |
| `is-none`  | `True` when the relation was explicitly scoped with `.none` |

```perl6
User.is-empty;                                 # nothing in the table?
User.where({active => True}).is-any;           # any active users?
User.where({email => $addr}).is-one;           # exactly one match?
```

`is-none` reports the *scope*, not the *result*: it is only true when the
relation has been passed through `.none`, never just because the result set
happens to be empty.

```perl6
User.where({fname => 'Nobody'}).is-none;       # False — query just returns nothing
User.none.is-none;                             # True  — explicitly null-scoped
```

A `.none`-scoped relation short-circuits every predicate without touching
the database. `is-any`, `is-one`, and `is-many` are always `False`;
`is-empty` is always `True`.

## cache-key, cache-version, cache-key-with-version

These produce stable cache fingerprints for a relation, shaped like this:

```text
cache-key                 →  "users/query-3f8a91c20b6e54de"
cache-version             →  "12-2026-05-12T14:23:11Z"
cache-key-with-version    →  "users/query-3f8a91c20b6e54de-12-2026-05-12T14:23:11Z"
```

`cache-key` is the table name plus an FNV-1a fingerprint of the SQL the
relation would emit (template *and* bind values). Two relations producing
the same SQL produce the same key; changing the WHERE, ORDER, or bind
values changes it.

`cache-version` summarizes the *result* of the query as
`<count>-<max(updated_at)>`. If the model has no `updated_at` column,
`cache-version` is undefined (`Str`) and `cache-key-with-version` falls
back to the bare `cache-key`. On a `.none`-scoped relation, the version
short-circuits to `"0"` without touching the database.

```perl6
my $key = User.where({active => True}).cache-key-with-version;

# Use it as a key in your own cache store:
my $payload = $cache.fetch-or-compute($key, {
  User.where({active => True}).all.map(*.serialize);
});
```

Because `cache-key-with-version` rolls forward whenever any row in the
relation is updated, a cache entry written under this key invalidates on the
next read.
