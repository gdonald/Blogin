---
title: Enums
slug: enums
order: 10
description: An enum maps a column's stored value to a set of symbolic names. The column holds the backing value (an integer or a string); the model works in terms of...
toc: true
---
An enum maps a column's stored value to a set of symbolic names. The column
holds the backing value (an integer or a string); the model works in terms of
the names.

## Declaring

Declare an enum in the model's `submethod BUILD`, alongside attributes,
validations, and associations. The first argument is the column; the second
maps each symbolic name to its backing value.

```perl6
class Order is Model {
  method table-name { 'orders' }

  submethod BUILD {
    self.enum: 'status', { active => 0, archived => 9 };
    self.enum: 'state',  { open => 'open', closed => 'closed' };
  }
}
```

Backing values may be integers or strings, and the integers need not be
contiguous. A column with no enum keeps its raw value.

## Reading and writing

The reader returns the symbolic name; the column stores the backing value:

```perl6
my $order = Order.create({ status => 'active' });
$order.status;          # 'active'
# the orders.status column holds 0
```

Assigning either a name or a backing value normalises to the name in memory:

```perl6
Order.create({ status => 9 }).status;    # 'archived'
```

## Predicates and the bang setter

Each value gets a predicate and a setter. The predicate `is-<value>` reports
whether that value is current. The setter `<value>-bang` assigns the value and
saves the record.

```perl6
$order.is-active;       # True
$order.is-archived;     # False

$order.archived-bang;   # sets status to 'archived' and saves
```

The bang suffix stands in for a trailing `!` (Raku method names cannot end in `!`).

## Class scopes

Each value adds a class scope returning a relation filtered to that value:

```perl6
Order.active.all;       # orders whose status is active
Order.archived.all;
Order.closed.all;       # works for text-backed enums too
```

## Inspecting

`enum-values('status')` lists the symbolic names declared for a column.

```perl6
Order.enum-values('status');    # ('active', 'archived')
```

## State machines

There is no built-in state-machine DSL — it is out of scope for the ORM core.
Model a state column with an enum and guard the transitions with
[validations](/docs/ORM-ActiveRecord/data) and [callbacks](/docs/ORM-ActiveRecord/callbacks), or reach
for a dedicated state-machine library when you need guards, events, and
transition callbacks as first-class concepts.
