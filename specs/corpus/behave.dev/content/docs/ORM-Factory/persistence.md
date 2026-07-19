---
title: Persistence
slug: persistence
order: 21
description: ORM::Factory is ORM-agnostic at its core. The build strategies (build, create, attributes-for, build-stubbed) target a small protocol —...
toc: true
---
`ORM::Factory` is ORM-agnostic at its core. The build strategies (`build`,
`create`, `attributes-for`, `build-stubbed`) target a small protocol —
`ORM::Factory::Persistence` — never a specific ORM directly.

## The protocol

```perl6
unit role ORM::Factory::Persistence;

method instantiate(Mu $class, %attrs)       { ... }
method persist(Mu $instance)                { ... }
method is-valid(Mu $instance --> Bool)      { True   }
method errors(Mu $instance)                 { Empty  }
method primary-key(Mu $class --> Str)       { 'id'   }
method stub(Mu $instance)                   { $instance }
```

- `instantiate` builds an unsaved instance from a class and an attribute
  hash. `build` calls this and stops here.
- `persist` saves the instance and returns it (raising if the ORM rejects
  it). `create` calls `instantiate` then `persist`.
- `is-valid` / `errors` expose validation state without committing.
- `primary-key` is the name of the model's primary key column.
- `stub` returns a frozen, no-DB-access stand-in. `build-stubbed` runs
  through this.

`attributes-for` deliberately bypasses the adapter entirely — it returns a
plain attribute hash without ever instantiating, persisting, or stubbing.

## The generic default

`ORM::Factory::Persistence::Generic` is the fallback adapter used when no
ORM-specific adapter is registered. It:

- instantiates via `$class.new(|%attrs)`;
- persists by calling `.save-bang` if present, else `.save`, else raising
  `X::ORM::Factory::Persistence::NoPersistence` with a clear hint;
- delegates `is-valid`, `errors`, `primary-key` to identically named methods
  on the instance / class if they exist, and falls back to sane defaults
  otherwise.

```perl6
use ORM::Factory::Persistence::Generic;

my $adapter = ORM::Factory::Persistence::Generic.new;
my $user    = $adapter.instantiate(User, %( fname => 'Greg' ));
$adapter.persist($user);          # invokes $user.save-bang or $user.save
```

## Detection and selection

On first access of `ORM::Factory.persistence`, the library:

1. checks for an explicit adapter set via `ORM::Factory.set-persistence`;
2. tries to `require ORM::Factory::Persistence::ActiveRecord` — if it loads,
   uses that adapter (this is how the `ORM::ActiveRecord` adapter
   auto-registers, with no `use` in your specs);
3. falls back to `ORM::Factory::Persistence::Generic`.

Once resolved, the adapter is cached for the rest of the process. Use
`ORM::Factory.reset-persistence` in tests to force re-detection.

```perl6
# default — auto-detected on first call
my $adapter = ORM::Factory.persistence;

# explicit, e.g. installing a custom adapter
ORM::Factory.set-persistence(MyAdapter.new);

# reset for the next test
ORM::Factory.reset-persistence;
```

## Writing a custom adapter

Implement the `ORM::Factory::Persistence` role and call `set-persistence`:

```perl6
use ORM::Factory::Persistence;

class MyOrmAdapter does ORM::Factory::Persistence {
  method instantiate(Mu $class, %attrs) { $class.from-hash(%attrs) }
  method persist(Mu $instance)          { $instance.commit;    $instance }
  method is-valid(Mu $instance)         { $instance.validate-ok }
  method errors(Mu $instance)           { $instance.validation-errors }
  method primary-key(Mu $class)         { $class.pk-name }
  method stub(Mu $instance)             { $instance.frozen-stub }
}

ORM::Factory.set-persistence(MyOrmAdapter.new);
```

Per-factory and global `to-create` / `initialize-with` / `skip-create` hooks
sit on top of this seam: they intercept before the adapter is consulted, and
fall through to the adapter when not present. See
[Construction](/docs/ORM-Factory/construction) for the full hook protocol and resolution
order.

For the bundled `ORM::ActiveRecord` adapter, see
[ORM::ActiveRecord adapter](/docs/ORM-Factory/active-record).
