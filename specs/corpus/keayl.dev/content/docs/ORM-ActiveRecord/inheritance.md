---
title: Inheritance
slug: inheritance
order: 13
description: Single-table inheritance (STI) stores a whole class hierarchy in one table. A base model owns the table; subclasses share it. A row's type column records...
toc: true
---
Single-table inheritance (STI) stores a whole class hierarchy in one table. A
base model owns the table; subclasses share it. A row's `type` column records
which subclass it is, so reads come back as the right class and a subclass
finder sees only its own rows.

## Setting it up

The base model maps to the table; subclasses inherit from it (and so inherit
its `table-name`). The table needs a `type` column.

```perl6
class Vehicle is Model { method table-name { 'vehicles' } }
class Car        is Vehicle { }
class Motorcycle is Vehicle { }
class SportsCar  is Car     { }
```

```perl6
# migration
self.create-table: 'vehicles', [
  type   => { :string, limit => 32 },
  name   => { :string, limit => 64 },
  wheels => { :integer, default => 0 },
];
```

A plain `type` column does not by itself make a model STI: a model with no
model superclass and no model subclasses is never treated as STI, so an
ordinary `type` attribute keeps working.

## How it behaves

**Writes** populate the type column with the saving class:

```perl6
Car.create({ name => 'Civic', wheels => 4 });   # type = 'Car'
```

**Reads** dispatch each row to the class named by its type column:

```perl6
Vehicle.all.perform;     # a Car and a Motorcycle, each its own class
```

**Subclass finders** scope to the subclass and its descendants; the base sees
everything:

```perl6
Car.all.perform;         # Cars and SportsCars
Vehicle.all.perform;     # every row
```

`becomes(Subclass)` re-casts an instance to another class in the hierarchy
keeping the same record; `becomes-bang` also rewrites the type column.

## Configuration

Configure a hierarchy by calling these class methods after the class
definition. A setter takes a value; with no argument the method is a getter.

| Method                            | Effect                                                  |
| --------------------------------- | ------------------------------------------------------- |
| `inheritance-column('kind')`      | Use a different column name (default `type`).           |
| `abstract-class(True)`            | Mark a class abstract: it owns no table and the first concrete class below it is the STI root. |
| `sti-name('automobile')`          | Override the value stored for this class.               |
| `store-full-sti-class(False)`     | Store the short (namespace-stripped) class name.        |

`inheritance-column` and `store-full-sti-class` are inherited by subclasses;
`abstract-class` and `sti-name` apply to the class they are set on.

```perl6
class Account is Model { method table-name { 'accounts' } }
Account.inheritance-column('kind');
Account.abstract-class(False);
```

`descends-from-active-record` is true for the STI root (and for abstract
classes) and false for an STI subclass:

```perl6
Vehicle.descends-from-active-record;    # True
Car.descends-from-active-record;        # False
```
