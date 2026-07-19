---
title: ORM::ActiveRecord adapter
slug: active-record
order: 22
description: ORM::Factory ships with a concrete adapter for ORM::ActiveRecord. It is auto-detected on first use: if ORM::ActiveRecord::Model is loadable in the...
toc: true
---
`ORM::Factory` ships with a concrete adapter for
[`ORM::ActiveRecord`](https://github.com/gdonald/ORM-ActiveRecord). It is
auto-detected on first use: if `ORM::ActiveRecord::Model` is loadable in the
process, the AR adapter is registered, otherwise `ORM::Factory` falls back to
the generic adapter.

## What the adapter wires up

`ORM::Factory::Persistence::ActiveRecord` implements the protocol from
[Persistence](/docs/ORM-Factory/persistence) on top of the AR model API:

- `instantiate` calls `Model.build(%attrs)`, returning an unsaved instance with
  `is-new-record === True`.
- `persist` calls `.save-bang`, so validation failures raise
  `X::RecordInvalid` from `ORM::ActiveRecord`.
- `is-valid` / `errors` delegate to the model's own validation pipeline.
- `primary-key` returns the model's primary key (`id` by default).
- `stub` fakes a positive id and populates `created_at` / `updated_at` without
  hitting the database — see `build-stubbed` below.

## `build`

`ORM::Factory.build('user')` returns an AR model with the attributes set but
no `INSERT` issued:

```perl6
my $u = ORM::Factory.build('user');

$u.is-new-record;          # True
$u.attrs<fname>;           # 'Greg'
```

## `create`

`ORM::Factory.create('user')` runs the same pipeline as `build`, then calls
`save-bang` through the adapter. Model validations, callbacks, and
timestamps all run as usual:

```perl6
my $u = ORM::Factory.create('user');

$u.is-persisted;           # True
$u.id;                     # > 0
$u.attrs<created_at>;      # DateTime, set by AR
```

If validation fails, `create` raises the AR exception unchanged:

```perl6
try {
  ORM::Factory.create('user', fname => Nil);   # presence-of fails
  CATCH {
    when X::RecordInvalid {
      .messages;                                       # ('fname can't be blank',)
    }
  }
}
```

## `build-stubbed`

`build-stubbed` returns a model that *looks* persisted but never touched the
database:

```perl6
my $u = ORM::Factory.build-stubbed('user');

$u.is-persisted;           # True
$u.is-new-record;          # False
$u.id;                     # positive, monotonically increasing
$u.attrs<created_at>;      # set
$u.attrs<updated_at>;      # set
```

Use it for tests that need a record with associations and timestamps but do
not exercise the persistence path. Each stub gets a unique id from a process
counter; call
`ORM::Factory::Persistence::ActiveRecord.new.reset-stub-counter` between
suites if you want stable ids.

## `attributes-for`

`attributes-for` bypasses the adapter entirely (see [Persistence](/docs/ORM-Factory/persistence)),
so it works identically whether AR is loaded or not.

## Auto-detection

Detection is lazy and happens on first read of `ORM::Factory.persistence`. The
resolution order is:

1. an adapter explicitly set via `ORM::Factory.set-persistence`;
2. `ORM::Factory::Persistence::ActiveRecord`, if `ORM::ActiveRecord::Model`
   loads;
3. `ORM::Factory::Persistence::Generic`.

The result is cached for the rest of the process. Use
`ORM::Factory.reset-persistence` (typically in test setup) to force a
re-detection.

```perl6
ORM::Factory.reset-persistence;
ORM::Factory.persistence.^name;
# 'ORM::Factory::Persistence::ActiveRecord' when AR is installed
# 'ORM::Factory::Persistence::Generic'      otherwise
```

## Test-suite cleanup

For tests, `ORM::Factory::Cleanup` exposes two strategies that target the AR
adapter directly:

```perl6
use ORM::Factory::Cleanup;

# Rolls back every change made inside the block, including failures
with-transaction-rollback {
  ORM::Factory.create-list('user', 3);
  ...
};

# Wipes specific tables (DELETE on SQLite, TRUNCATE ... CASCADE on Postgres,
# TRUNCATE with FK checks toggled on MySQL)
truncate-tables('users', 'posts');

# Wipes every table except `migrations`
truncate-all-tables();
```

`with-transaction-rollback` is the lighter-weight option for fast specs; use
`truncate-tables` when the test cannot wrap itself in a single transaction
(e.g. specs that themselves open a transaction).

## What the adapter does *not* do

The AR adapter targets the persistence protocol and nothing more. Anything
specific to the model — validations, callbacks, association metadata,
timestamps — is the model's responsibility. The adapter just forwards build /
save / validate calls and fakes ids for stubs.
