---
title: Async queries
slug: async
order: 20
description: Every read method has an -async variant that runs the query on a worker thread and returns a Raku Promise. await it (or call .result) when you need the...
toc: true
---
Every read method has an `-async` variant that runs the query on a worker
thread and returns a Raku `Promise`. `await` it (or call `.result`) when you
need the value. The point is to fire several independent queries and let them
run concurrently instead of one after another.

```perl6
my $users-p = User.where({ active => True }).load-async;
my $count-p = Order.all.count-async;

# … other work while both queries run …

my @users = await $users-p;
my $count = await $count-p;
```

## What's available

| Sync | Async |
| ---- | ----- |
| `all` / `perform` | `load-async` |
| `count` | `count-async` |
| `sum` / `average` / `minimum` / `maximum` | `sum-async` / `average-async` / `minimum-async` / `maximum-async` |
| `calculate` | `calculate-async` |
| `pluck` / `pick` / `ids` | `pluck-async` / `pick-async` / `ids-async` |
| `find-by-sql` | `find-by-sql-async` |

They exist both on a relation and at the class level:

```perl6
User.where({ active => True }).load-async;   # on a relation
User.count-async;                            # class level (whole table)
User.find-by-sql-async('SELECT * FROM users WHERE age > ?', 18);
```

## How it runs

Each async call checks out a dedicated connection from the connection pool and
runs the whole query — the SELECT and the object instantiation — on that
connection on a worker thread, then returns it to the pool. Database driver
handles are not thread-safe, so the async work never touches the shared
connection the main thread is using; that is what makes it safe to run queries
in parallel.

Records returned by `load-async` and `find-by-sql-async` are rebound to the
shared connection before they reach you, so you can `save`, `reload`, and follow
associations on them from the calling thread as usual.

The number of queries that truly run in parallel is bounded by the connection
pool size (the `pool` key in the connection config, default 5). Beyond that,
async calls wait for a connection to free up.

```json
{ "production": { "primary": { "adapter": "pg", "name": "ar", "pool": 10 } } }
```

## Errors

A query that fails breaks its `Promise`; `await` rethrows the exception on the
calling thread:

```perl6
my $p = User.find-by-sql-async('SELECT * FROM nope');
my @rows = await $p;   # rethrows the adapter error here
```
