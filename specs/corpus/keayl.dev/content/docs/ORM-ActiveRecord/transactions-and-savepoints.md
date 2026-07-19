---
title: Transactions and savepoints
slug: transactions-and-savepoints
order: 37
description: Wrap a unit of work in a transaction so it commits all-or-nothing. Nested transactions become savepoints, so an inner failure can be contained without...
toc: true
---
Wrap a unit of work in a transaction so it commits all-or-nothing. Nested
transactions become savepoints, so an inner failure can be contained without
losing the outer work.

See [Transactions](/docs/ORM-ActiveRecord/transactions) for the reference; this is a
working walkthrough.

## A basic transaction

Call `transaction` with a block on `DB.shared` (or on any model). It commits on
normal return and returns the block's value:

```perl6
use ORM::ActiveRecord::DB;

DB.shared.transaction({
  Widget.create({ name => 'committed', qty => 1 });
});
```

A model exposes the same wrapper:

```perl6
Widget.transaction({
  Widget.create({ name => 'via-model', qty => 5 });
});
```

## Rolling back

Throw `X::Rollback` to abort. It rolls the transaction back and is swallowed (it
does not propagate out of the block):

```perl6
use ORM::ActiveRecord::Errors::X;

DB.shared.transaction({
  Widget.create({ name => 'aborted', qty => 1 });
  die X::Rollback.new(:reason<changed-my-mind>);
});

DB.shared.is-in-transaction;   # False — nothing persisted
```

Any other exception also rolls the transaction back, but rethrows so the caller
sees it.

## Nested savepoints with :requires-new

A plain nested `transaction` joins the outer one. Pass `:requires-new` to open a
savepoint instead, so the inner block can fail on its own:

```perl6
DB.shared.transaction({
  Widget.create({ name => 'outer', qty => 1 });

  DB.shared.transaction(:requires-new, {
    Widget.create({ name => 'doomed', qty => 9 });
    die X::Rollback.new;          # rolls back to the savepoint only
  });

  Widget.create({ name => 'after', qty => 3 });
});
# 'outer' and 'after' persist; 'doomed' does not.
```

## Isolation levels

The outermost transaction can request an isolation level (it is an error on a
nested one):

```perl6
DB.shared.transaction(:isolation<read_committed>, {
  # ...
});
```

`read_uncommitted`, `read_committed`, `repeatable_read`, and `serializable` are
accepted.

## After-commit / after-rollback callbacks

Register transactional callbacks on a model in `BUILD`; they fire after the
outer transaction settles, not when the record is written:

```perl6
class Widget is Model {
  submethod BUILD {
    self.after-commit:   -> { note "committed {self.attrs<name>}" };
    self.after-rollback: -> { note "rolled back {self.attrs<name>}" };
  }
}
```

There are also `after-create-commit`, `after-update-commit`,
`after-destroy-commit`, and `after-save-commit`. A record written inside a
savepoint that rolls back fires `after-rollback`, while the surrounding
committed records still fire `after-commit`.
