---
title: Reflection
slug: reflection
order: 40
description: Public class-level introspection so tools (a console, a serializer, a factory) can build, stub, and describe records without reaching into model...
toc: true
---
Public class-level introspection so tools (a console, a serializer, a factory)
can build, stub, and describe records without reaching into model internals.
Every method works on the class or on an instance.

## Associations

```perl6
Author.association-names;              # ('books', 'reviews')
Author.associations;                   # a reflection per association
my $ref = Author.reflect-on-association('books');
```

A reflection (`AssociationReflection`) describes one declared association:

| Method | Meaning |
| ------ | ------- |
| `name` | the association name |
| `macro` | `belongs-to`, `has-many`, `has-one`, `has-and-belongs-to-many` |
| `klass` | the resolved target class (`Mu` for a polymorphic / unresolved one) |
| `class-name` | the target class name |
| `foreign-key` | the foreign key column |
| `primary-key` | the primary key on the target |
| `polymorphic` | `True` for a polymorphic `belongs-to` |
| `through` | the join association name, for `:through` |
| `source` | the source association name, for `:through` |
| `is-collection` / `is-singular` | plural vs singular |

```perl6
my $ref = Book.reflect-on-association('cover');
$ref.macro;          # 'belongs-to'
$ref.polymorphic;    # True
$ref.foreign-key;    # 'cover_id'
$ref.is-singular;    # True
```

## Columns and attributes

```perl6
Author.column-names;          # ('id', 'name', 'role', 'created_at', 'updated_at')
Author.columns;               # a hash per column: name, type, null, default
Author.column('name');        # { name => 'name', type => 'text', null => True, ... }
Author.column-type('role');   # 'integer'
```

`columns` reads live metadata from the adapter, so `null` and `default` reflect
the actual schema.

## Primary key

```perl6
Author.primary-keys;          # ('id',) — a list, composite-aware
Author.primary-key-type;      # 'integer'
Author.has-composite-primary-key;
```

## Enums

```perl6
Author.enums;                 # { role => { admin => 0, user => 1 } }
Author.enums<role><admin>;    # 0
```

## Construction contract

These are the canonical "instantiate from a hash without saving" entry points
that adapters target:

```perl6
my $author = Author.build({ name => 'Ada' });   # unsaved instance, no write
$author.assign-attributes({ role => 'admin' }); # set more attributes
$author.is-new-record;                           # True
```

`build` constructs an instance with `id` 0 and applies attribute types; it never
touches the database. `assign-attributes` merges a hash into an existing
instance.

## Stubbed records

`build-stubbed` returns an instance that reports as persisted — with a fake id
and timestamps — without any write, for tests and previews that must not hit the
database:

```perl6
my $stub = Author.build-stubbed({ name => 'Ada' });
$stub.is-persisted;          # True
$stub.attrs<created_at>;     # set

$stub.save;                  # raises X::ReadOnlyRecord
$stub.reload;                # raises X::StubbedRecord
```

A stub is read-only (it raises on `save` / `update` / `destroy`) and guards
against a `reload`, so a stubbed record can never silently turn into a database
round-trip.
