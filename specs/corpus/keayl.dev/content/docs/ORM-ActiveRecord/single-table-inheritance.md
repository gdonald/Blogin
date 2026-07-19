---
title: Single-table inheritance
slug: single-table-inheritance
order: 35
description: One table holds several related classes, told apart by a type column. A Vehicle table stores cars and motorcycles; each row remembers which class it is...
toc: true
---
One table holds several related classes, told apart by a `type` column. A
`Vehicle` table stores cars and motorcycles; each row remembers which class it
is and comes back as that class.

See [Inheritance](/docs/ORM-ActiveRecord/inheritance) for the full reference; this is a
working walkthrough.

## Setup

Give the table a discriminator column (`type` by default) and the columns every
subclass shares:

```perl6
self.create-table: 'vehicles', [
  type   => { :string, limit => 32 },
  name   => { :string, limit => 64 },
  wheels => { :integer, default => 0 },
];
```

The base maps to the table; subclasses just inherit:

```perl6
class Vehicle is Model { method table-name { 'vehicles' } }
class Car        is Vehicle { }
class Motorcycle is Vehicle { }
class SportsCar  is Car     { }
```

## Saving and reading back

Creating through a subclass stamps the `type` column:

```perl6
my $car = Car.create({ name => 'Civic', wheels => 4 });
$car.attrs<type>;                # Car.sti-name
```

Querying the base returns each row as its own subclass:

```perl6
Car.create({ name => 'Civic', wheels => 4 });
Motorcycle.create({ name => 'Harley', wheels => 2 });

my @all = Vehicle.all.perform;
@all.grep(* ~~ Car).elems;          # 1
@all.grep(* ~~ Motorcycle).elems;   # 1
```

A subclass finder is scoped to its own type:

```perl6
Car.all.perform.elems;   # 1 — only the Civic
```

## Changing a row's class

`becomes-bang` rewrites the `type` column to another subclass:

```perl6
my $moto = Car.create({ name => 'x', wheels => 4 }).becomes-bang(Motorcycle);
$moto.attrs<type>;   # Motorcycle.sti-name
```

## Tuning the column and names

Each knob is a class-level setter (with a matching getter):

```perl6
Gizmo.inheritance-column('kind');       # use a different discriminator column
Tool.abstract-class(True);              # abstract base; first concrete subclass is the root
Motorcycle.sti-name('moto');            # store a custom value instead of the class name
Catalog.store-full-sti-class(False);    # store the short, namespace-stripped name
```

```perl6
Vehicle.inheritance-column;   # 'type'
Gizmo.inheritance-column;     # 'kind' (inherited by its subclasses)
Tool.abstract-class;          # True
```
