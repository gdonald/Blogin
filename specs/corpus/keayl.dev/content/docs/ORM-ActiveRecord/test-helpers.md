---
title: Test helpers
slug: test-helpers
order: 44
description: Three helpers under ORM::ActiveRecord::Testing keep test data isolated and easy to set up: a transactional wrapper, a YAML fixture loader, and a database...
toc: true
---
Three helpers under `ORM::ActiveRecord::Testing` keep test data isolated and
easy to set up: a transactional wrapper, a YAML fixture loader, and a database
cleaner.

## Transactional tests

Wrapping each example in a transaction that is rolled back at teardown leaves
the database untouched between tests, with no `DELETE` or `TRUNCATE`. Nested
model transactions join the open one (via savepoints), so a test's own
`transaction { ... }` still behaves.

```perl6
use ORM::ActiveRecord::Testing::Transaction;

before-each { begin-transactional-test }
after-each  { rollback-transactional-test }
```

Or wrap a single body:

```perl6
with-rollback {
  User.create({ fname => 'Ada' });
  expect(User.count).to.eq(1);
};   # the row is gone here
```

`begin-transactional-test` accepts an `:isolation` level
(`READ COMMITTED`, `REPEATABLE READ`, ...).

## Fixtures

Fixtures are YAML files, one per table (the filename is the table name). Each
top-level key is a *label*; its id is derived deterministically from the label,
so references resolve across files regardless of load order.

```yaml
# authors.yml
alice:
  name: Alice
  admin: <%= True %>
bob:
  name: Bob
```

```yaml
# articles.yml
hello:
  title: Hello World
  author: alice          # belongs-to: author_id = identify('alice')
```

```perl6
use ORM::ActiveRecord::Testing::Fixtures;

my $fx = Fixtures.new(dir => 'test/fixtures').load;

$fx.id('authors', 'alice');        # the deterministic id
$fx.record('articles', 'hello');   # the inserted row hash
$fx.labels('authors');             # ('alice', 'bob')
```

Loading deletes the table's existing rows first, so it is repeatable.

### Interpolation

`<%= expr %>` in a value is replaced by the stringified result of the Raku
expression before the YAML is parsed.

```yaml
created_at: <%= DateTime.now %>
admin: <%= True %>
```

### References between files

A key that is not a column but whose `<key>_id` *is* a column is treated as a
`belongs-to` reference: the value names another fixture's label, and the loader
sets `<key>_id` to that label's deterministic id. Because the id comes from the
label alone, the referenced file need not be loaded first (or at all).

```perl6
Fixtures.new.identify('alice');    # the same id 'author: alice' resolves to
```

## Database cleaner

When you are not using the transactional wrapper, clean explicitly:

```perl6
use ORM::ActiveRecord::Testing::DatabaseCleaner;

my $cleaner = DatabaseCleaner.new;

$cleaner.clean;                          # deletion (DELETE FROM every table)
$cleaner.clean(strategy => 'truncation');# TRUNCATE / reset identities
```

`truncation` issues `TRUNCATE ... RESTART IDENTITY CASCADE` on PostgreSQL,
disables foreign-key checks around `TRUNCATE` on MySQL, and falls back to
`DELETE` (plus clearing `sqlite_sequence`) on SQLite.

The `transaction` strategy runs a block and rolls it back, like the wrapper:

```perl6
$cleaner.cleaning({
  # writes here are discarded
}, strategy => 'transaction');
```

The `migrations` table is never touched. Pass `except => <migrations other>` to
spare more tables.
