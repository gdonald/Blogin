---
title: Transactions
slug: transactions
order: 25
description: ORM::ActiveRecord exposes transactions through a block helper that opens a SQL transaction, runs the block, and commits if the block returns normally. If...
toc: true
---
ORM::ActiveRecord exposes transactions through a block helper that opens a
SQL transaction, runs the block, and commits if the block returns normally.
If the block dies, the helper rolls back and re-raises the exception. If the
block raises `X::Rollback`, the helper rolls back and returns `Nil` without
re-raising — that's the way to signal "abandon this work" without taking
down the surrounding code.

## Basic Usage

```perl6
use ORM::ActiveRecord::DB;

DB.shared.transaction({
  $account.update({ balance => $account.balance - 100 });
  $other.update({ balance => $other.balance + 100 });
});
```

If either `update` raises, the transaction rolls back and the exception
propagates. If both succeed, the transaction commits.

`Model.transaction` is a thin wrapper over `DB.shared.transaction` for
ergonomics:

```perl6
Account.transaction({
  Account.create({ name => 'a' });
  Account.create({ name => 'b' });
});
```

The block's return value flows back through the helper:

```perl6
my $result = DB.shared.transaction({ Account.count });
```

## Explicit Rollback

To roll back without surfacing an error to callers, raise `X::Rollback`.
The helper swallows it after the rollback and returns `Nil`.

```perl6
use ORM::ActiveRecord::Errors::X;

DB.shared.transaction({
  $order.update({ status => 'cancelled' });

  if $payment-gateway.refund-failed {
    die X::Rollback.new(:reason<refund-failed>);
  }
});
```

## Nested Transactions and Savepoints

By default, an inner `transaction` call joins the surrounding transaction:
the inner block runs in the same SQL transaction, and an exception inside
it rolls back the whole outer transaction.

```perl6
DB.shared.transaction({
  $a.save;
  DB.shared.transaction({
    $b.save;
    die 'boom';   # rolls back $a AND $b
  });
});
```

Pass `:requires-new` to wrap the inner block in a `SAVEPOINT` instead.
An `X::Rollback` inside `:requires-new` only undoes the savepoint; work
done in the outer transaction is preserved.

```perl6
DB.shared.transaction({
  $a.save;
  DB.shared.transaction(:requires-new, {
    $b.save;
    die X::Rollback.new;   # rolls back $b only
  });
  $c.save;   # this still commits with $a
});
```

Other exceptions inside a `:requires-new` block still roll back the
savepoint, but they re-raise and propagate to the outer transaction —
which then rolls back everything in turn.

## Isolation Level

The outermost transaction accepts an `:isolation` option naming a
SQL-standard isolation level. Underscores or spaces both work, in any
case:

```perl6
DB.shared.transaction(:isolation<read_committed>, {
  ...
});

DB.shared.transaction(:isolation<'SERIALIZABLE'>, {
  ...
});
```

Accepted values: `read uncommitted`, `read committed`, `repeatable read`,
`serializable`. Unknown levels fail fast.

Isolation is only valid on the **outermost** call. Passing `:isolation`
to a nested `transaction` raises — savepoints inherit the surrounding
transaction's isolation level.

Per-adapter notes:

- **PostgreSQL** emits `BEGIN ISOLATION LEVEL …`.
- **MySQL** emits `SET TRANSACTION ISOLATION LEVEL …` followed by
  `START TRANSACTION` (MySQL doesn't accept the level inside `START
  TRANSACTION`).
- **SQLite** accepts and validates the keyword, but ignores it — SQLite
  doesn't expose SQL-standard isolation levels.

## Introspection

`DB.shared.is-in-transaction` reports whether a transaction is open on
the shared connection. Useful from callback code that needs to know
whether it's running inside a wrapping `transaction { ... }` block:

```perl6
self.after-save: -> {
  return unless DB.shared.is-in-transaction;
  # ... defer something until commit ...
};
```

`DB.shared.txn-depth` exposes the current depth (`0` outside any
transaction, `1` inside the outer block, `>1` inside a `:requires-new`
savepoint).

## Transactional Callbacks

Models can register callbacks that fire only after a transaction's outcome
is decided. These are different from `after-save` / `after-destroy`, which
fire as soon as the row hits the database — those run inside the
transaction and would still execute even if the surrounding transaction
later rolls back.

The transactional variants are:

- `after-commit` — fires on commit, regardless of which action ran
- `after-rollback` — fires on rollback
- `after-create-commit` — fires on commit, only if the record was created
- `after-update-commit` — fires on commit, only if the record was updated
- `after-destroy-commit` — fires on commit, only if the record was destroyed
- `after-save-commit` — fires on commit for either create or update

Register them the same way as the non-transactional callbacks, in `BUILD`:

```perl6
class Order is Model {
  submethod BUILD {
    self.after-commit:        -> { Audit.log("order touched: " ~ self.id) };
    self.after-create-commit: -> { Mailer.send-receipt(self) };
    self.after-rollback:      -> { Audit.log("order rolled back: " ~ self.id) };
  }
}
```

Outside an explicit `transaction { ... }` block, the commit callbacks fire
immediately after the save lands (since the implicit per-statement
transaction has already committed). Inside an explicit block, they're
deferred until the outermost `transaction` commits.

If the same record is saved multiple times in one transaction, each commit
callback fires once. A create followed by an update in the same
transaction fires `after-create-commit` and `after-save-commit`, not
`after-update-commit` — the record is still a fresh create from the
transaction's perspective.

Savepoints get their own callback frame:

- A `:requires-new` block that completes successfully merges its pending
  callbacks into the parent frame, so they fire at the outermost commit.
- A `:requires-new` block that rolls back (via `X::Rollback` or an
  exception) fires `after-rollback` for the records inside it
  immediately, and drops their commit callbacks.

## Optimistic locking

Add an integer `lock_version` column (default `0`) to a table and ORM::ActiveRecord
will check-and-bump it on every update. The save succeeds only when the row in
the database still has the same `lock_version` the in-memory record was loaded
with; otherwise the save raises `X::StaleObjectError` and the row is left
untouched.

```raku
class Note is Model {}

my $a = Note.create({ title => 'one' });
my $stale = Note.find($a.id);

$a.title = 'winner';
$a.save;                       # lock_version: 0 -> 1

$stale.title = 'loser';
$stale.save;                   # raises X::StaleObjectError

$stale.reload;                 # pulls the winning row back in
$stale.title = 'after reload';
$stale.save;                   # lock_version: 1 -> 2
```

Detection is column-driven: `Note.new.is-locking-enabled` returns `True` when
the table has the `lock_version` column. Tables without the column behave like
they always did — no version check, no bump.

### Bulk updates

`update-all` and `update-counters` also bump `lock_version` when the column
exists, so in-memory records that were loaded before the bulk update become
stale on their next save:

```raku
Note.update-all({ views => 5 });                  # lock_version += 1 on every row
Note.where({ id => $id }).update-counters(views => 1);

# An explicit lock_version in the attrs hash wins; nothing else is bumped:
Note.where({ id => $id }).update-all({ lock_version => 42 });
```

The bump fires once per row regardless of how many columns the update touches,
matching the per-save behavior on individual records.

## Pessimistic locking

Where optimistic locking detects a conflict *after* the fact, pessimistic
locking acquires a row-level lock at read time so other transactions are
forced to wait. ORM::ActiveRecord exposes three entry points:

- `Model.lock` / `relation.lock` — chainable; appends `FOR UPDATE` to the
  next SELECT
- `record.lock-bang` — re-fetches the receiver with `FOR UPDATE`
- `record.with-lock { ... }` — opens a transaction, locks the receiver,
  yields the freshly-locked record to the block

```raku
Account.transaction({
  my $account = Account.lock.find($id);   # SELECT ... FOR UPDATE
  $account.update({ balance => $account.balance - 100 });
});
```

`lock-bang` is the per-record reload form. It reads the row again with a
lock, copies the row's current attrs onto the receiver, and raises
`X::RecordNotFound` if the row is gone. It must be called inside a
transaction — without one the lock is released the moment the SELECT
returns.

```raku
Account.transaction({
  $account.lock-bang;
  $account.update({ balance => $account.balance - 100 });
});
```

`with-lock` packages the common case — a transaction plus a locking
reload — into one call:

```raku
$account.with-lock(-> $locked {
  $locked.update({ balance => $locked.balance - 100 });
});
```

The block receives the freshly-locked record. If the block returns
normally the transaction commits; if it raises, the transaction rolls
back and the exception propagates.

### Lock modes

`lock` accepts an optional mode string that's emitted verbatim, so any
adapter-specific clause works without library support:

```raku
Account.lock('FOR SHARE').where(:active).all;
Account.lock('FOR NO KEY UPDATE').find($id);   # Postgres
Account.lock('FOR UPDATE NOWAIT').find($id);   # Postgres / MySQL 8+
Account.lock('FOR UPDATE SKIP LOCKED').find($id);
```

`lock(True)` (the default) is `FOR UPDATE`; `lock(False)` clears any
prior lock setting. `unscope(:lock)` removes the lock from a chained
relation.

Per-adapter notes:

- **PostgreSQL** supports `FOR UPDATE`, `FOR NO KEY UPDATE`, `FOR SHARE`,
  `FOR KEY SHARE`, and the `NOWAIT` / `SKIP LOCKED` modifiers.
- **MySQL** supports `FOR UPDATE` and `FOR SHARE` (8.0+); older versions
  used `LOCK IN SHARE MODE`, which still works.
- **SQLite** has no row-level lock syntax — the entire database is locked
  by the surrounding transaction. The library quietly drops the clause
  so cross-adapter code keeps running.

## Suppression and write guards

Three block-scoped guards let you reason about *what may be written* without
having to thread flags through every call site. They're stack-friendly:
nesting works, and the flag is always cleared on exit, including when the
block dies.

### `Model.suppress`

`Model.suppress({ ... })` short-circuits `save` inside the block. The
record's in-memory attributes still update, but no `INSERT` or `UPDATE` is
emitted and `save` returns `True`. Useful when a callback chain would
normally produce a side-record (audit row, notification) that you don't want
this code path to write.

```raku
Note.suppress({
  my $obj = Note.create({ title => 'ghost' });
  $obj.id;     # 0 — never persisted
  Note.count;  # unchanged
});
```

`Note.is-suppressed` reports the current state. Suppression is scoped to
the class it's called on — `Note.suppress` does not affect `Memo.create`.
Nested `Note.suppress` blocks compose: the outer block stays suppressed
after the inner block returns.

### `while-preventing-writes`

`DB.shared.while-preventing-writes({ ... })` rejects *any* write SQL that
reaches the adapter inside the block. `INSERT`, `UPDATE`, `DELETE`,
`REPLACE`, `TRUNCATE`, and `MERGE` raise `X::ReadOnlyDatabase`; `SELECT`
and other read queries are unaffected. CTEs that wrap a data-modifying
statement (`WITH foo AS (DELETE ... RETURNING *) SELECT ...`, as emitted
by Postgres `delete-records`) are detected and rejected too.

```raku
DB.shared.while-preventing-writes({
  Account.count;            # ok — SELECT
  Account.create({...});    # raises X::ReadOnlyDatabase
});
```

This is the replica-role guard: turn it on around code paths you only want
to read from a read replica, and the guard surfaces any accidental write
instead of silently routing it to the wrong place.

`DB.shared.is-preventing-writes` reports the current state; nesting and
exception unwinding work the same way as `Model.suppress`.

### `prohibit-shard-swapping` / `prohibit-replica-swapping`

`DB.shared.prohibit-shard-swapping({ ... })` and
`DB.shared.prohibit-replica-swapping({ ... })` set independent block-scoped
flags that downstream code can consult before changing the connection's
shard or replica role. The two flags don't interact — entering one does
not raise the other — and both clear cleanly on block exit, even on
exception. The accompanying predicates are `is-shard-swapping-prohibited`
and `is-replica-swapping-prohibited`. These hooks exist so application
code (and the planned connection pool) can refuse to swap when the
surrounding scope has declared a hard pin.

## Limitations

`DB.shared` wraps a single shared connection, so transactions are
process-wide on that connection. Concurrent threads sharing the
connection share the same transaction. Connection-pool-aware
per-thread/per-fiber transaction tracking is on the roadmap alongside
the pool itself.
