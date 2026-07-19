---
title: Soft deletes
slug: discard
order: 26
description: soft-deletes turns destruction into a reversible flag. Instead of issuing a DELETE, a discarded record stamps a timestamp column and stays in the table,...
toc: true
---
`soft-deletes` turns destruction into a reversible flag. Instead of issuing a
`DELETE`, a discarded record stamps a timestamp column and stays in the table,
so it can be restored later.

```perl6
use ORM::ActiveRecord::Model;

class Notice is Model {
  submethod BUILD {
    self.soft-deletes;
  }
}
```

The column defaults to `deleted_at`. Pass `:column` to use another:

```perl6
self.soft-deletes(:column<discarded_at>);
```

The column must exist in the table and be nullable:

```perl6
self.create-table: 'notices', [
  name       => { :string, limit => 64 },
  deleted_at => { :datetime },
];
```

## Discarding and restoring

```perl6
my $notice = Notice.create({name => 'sale'});

$notice.discard;        # stamps deleted_at, returns True
$notice.is-discarded;   # True
$notice.is-kept;        # False

$notice.undiscard;      # clears deleted_at, returns True
$notice.is-kept;        # True
```

`discard` returns `False` without touching the row when it is already
discarded. `undiscard` returns `False` when the record is not discarded. The row
is never removed, so `discard` and `undiscard` round-trip the same record.

## Scopes

Three class scopes filter on the timestamp column:

```perl6
Notice.kept;            # rows where the column IS NULL
Notice.discarded;       # rows where the column IS NOT NULL
Notice.with-discarded;  # every row, ignoring the default scope
```

Each returns a relation, so the usual chaining applies
(`Notice.kept.where(...).order(...)`).

## Bulk operations

```perl6
Notice.discard-all;     # discards every kept row, returns the discarded records
Notice.undiscard-all;   # restores every discarded row, returns the restored records
```

Both run per-record, so discard callbacks fire for each row.

## Callbacks

`before-discard`, `after-discard`, `before-undiscard`, and `after-undiscard`
wrap the state change. A `before-discard` that returns `False` aborts the
discard, leaving the record kept.

```perl6
class Notice is Model {
  submethod BUILD {
    self.soft-deletes;

    self.before-discard:  -> { self.snapshot };
    self.after-discard:   -> { self.notify-archived };
    self.after-undiscard: -> { self.notify-restored };
  }
}
```

See [Callbacks](/docs/ORM-ActiveRecord/callbacks) for the handler forms (block or method name) and
the `:if` / `:unless` options, which apply here too.

## Default scope

By default the scopes are opt-in and the plain relation still returns every row.
Pass `:default-scope` to hide discarded rows from the default relation:

```perl6
class Parcel is Model {
  submethod BUILD {
    self.soft-deletes(:column<discarded_at>, :default-scope);
  }
}

Parcel.all;             # only kept rows
Parcel.with-discarded;  # every row
Parcel.discarded;       # only discarded rows
```

`with-discarded` and `unscope(where => 'discarded_at')` lift the hidden filter
when you need the full set. The default scope applies to relation queries
(`all`, `where`, associations); a direct `find($id)` still locates a discarded
row by primary key.
