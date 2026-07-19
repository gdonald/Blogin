---
title: Strategies
slug: strategy
order: 16
description: A *strategy* is the object that decides what build, create, attributes-for, and build-stubbed actually do. Each public method on ORM::Factory is a thin...
toc: true
---
A *strategy* is the object that decides what `build`, `create`,
`attributes-for`, and `build-stubbed` actually do. Each public method on
`ORM::Factory` is a thin wrapper that selects a strategy and delegates to
its `result` method.

## The role contract

`ORM::Factory::Strategy` is a role with three required methods:

| method                                       | purpose                                            |
| -------------------------------------------- | -------------------------------------------------- |
| `to-sym(--> Str)`                            | one of `build`, `create`, `attributes-for`, `build-stubbed` |
| `result(Evaluator $eval)`                    | produces the value the public method returns      |
| `association(Str $name, @variants, %opts)` | cascade an association to the chosen strategy     |

Every strategy carries a [`Persistence`](/docs/ORM-Factory/persistence) adapter so it can
instantiate, persist, or stub through the protocol rather than poking the
class directly.

## The built-in strategies

| strategy                  | `to-sym`        | `result` returns                                  |
| ------------------------- | --------------- | ------------------------------------------------ |
| `BuildStrategy`           | `build`         | a new instance, unsaved                          |
| `CreateStrategy`          | `create`        | a new instance, persisted via the adapter        |
| `AttributesForStrategy`   | `attributes-for`| a `Hash` of resolved (non-transient, non-assoc) attrs |
| `BuildStubbedStrategy`    | `build-stubbed` | a stubbed instance (adapter-faked id, no DB)     |

`ORM::Factory.strategy-for($name)` returns a fresh instance of the strategy
keyed by symbol — handy when wiring per-association overrides (see
[Associations](/docs/ORM-Factory/associations)).

## Association cascade

`association(...)` is the strategy hook that handles cascading. The default
implementations forward to the matching public method:

| strategy                | cascade target                |
| ----------------------- | ----------------------------- |
| `BuildStrategy`         | `ORM::Factory.build`          |
| `CreateStrategy`        | `ORM::Factory.create`         |
| `BuildStubbedStrategy`  | `ORM::Factory.build-stubbed`  |
| `AttributesForStrategy` | returns `Nil` (associations excluded) |

This is why `build('post')` builds the author, `create('post')` creates it,
and `attributes-for('post')` simply omits the column.

The cascade is also controlled by the `use-parent-strategy`
[configuration](/docs/ORM-Factory/configuration) flag — when `False`, associations default
to `create` no matter what the surrounding call was.

## Registering custom strategies

The four built-ins are pre-populated in the strategy registry. You can add
your own (or replace a built-in) with
[`register-strategy`](/docs/ORM-Factory/custom-strategies):

```raku
ORM::Factory.register-strategy('json', JsonStrategy);
ORM::Factory.json('user');  # dispatched via FALLBACK
```
