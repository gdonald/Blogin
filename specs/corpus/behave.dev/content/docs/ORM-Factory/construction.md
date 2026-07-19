---
title: Construction
slug: construction
order: 18
description: ORM::Factory is ORM-agnostic at its core — every build strategy routes instantiation and persistence through the Persistence adapter. Three hooks override...
toc: true
---
`ORM::Factory` is ORM-agnostic at its core — every build strategy routes
instantiation and persistence through the [Persistence adapter](/docs/ORM-Factory/persistence).
Three hooks override that adapter when the default behaviour does not fit:

- `initialize-with` — replaces the *constructor* (the `instantiate` call).
- `to-create` — replaces the *persistence* (the `persist` call on `create`).
- `skip-create` — turns `create` into `build` (no persistence at all).

Each hook can be set per-factory (inside the `factory` block), globally
(at the top level of `define`), or both. Per-factory wins over
global; child factories inherit the parent hook but can override it.

## `initialize-with`

By default `build` / `create` / `build-stubbed` instantiate via the
adapter's `instantiate`, which calls `$class.new(|%attrs)`. Override that with
a per-factory hook:

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
    .role:  'member';

    .initialize-with: -> $eval {
      User.new(|$eval.attributes, :via('hook'));
    };
  };
};

ORM::Factory.build('user').via;   # 'hook'
```

The block receives the [evaluator](/docs/ORM-Factory/transient) and returns the instance.
`attributes-for` deliberately bypasses the hook — it returns a plain hash.

### The `attributes` helper

Inside the hook, `$eval.attributes` is the resolved persisted-attribute hash
(transients and association attributes excluded — the same view
`attributes-for` returns). Use it to delegate to a constructor that expects
positional args or a different keyword shape:

```perl6
.initialize-with: -> $eval {
  User.from-hash($eval.attributes);
};
```

### Global `initialize-with`

A top-level `initialize-with` applies to every factory that has no
per-factory hook:

```perl6
define {
  .initialize-with: -> $eval {
    $eval.factory.lookup-class.new(|$eval.attributes, :via('global'));
  };

  .factory: 'user', { .fname: 'Greg' };
};

ORM::Factory.build('user').via;   # 'global'
```

`ORM::Factory.global-initialize-with` returns the registered global hook;
`ORM::Factory.reload` clears it.

## `to-create`

`to-create` replaces the persistence step in `create`. The default adapter
calls `.save-bang` (or `.save`) on the instance; the hook takes the instance
and the evaluator and is responsible for persisting it however the target
store wants:

```perl6
define {
  .factory: 'document', {
    .title: 'Hello';

    .to-create: -> $doc, $eval {
      $mongo.insert($doc.title);
      $doc;
    };
  };
};
```

`before-create` fires *before* `to-create`; `after-create` fires after. `build`
and `build-stubbed` never consult `to-create`.

### Global `to-create`

```perl6
define {
  .to-create: -> $instance, $eval {
    $persistence-bus.send($instance);
  };

  .factory: 'user', { .fname: 'Greg' };
};
```

`ORM::Factory.global-to-create` returns the registered global hook;
`ORM::Factory.reload` clears it.

## `skip-create`

`skip-create` makes `create` behave like `build` — no persistence happens, but
every callback still fires:

```perl6
define {
  .factory: 'ephemeral', {
    .title: 'in-memory only';

    .skip-create;
  };
};

ORM::Factory.create('ephemeral').saved;   # falsy — never persisted
```

A common shape is a global `skip-create` plus the few factories that *do*
persist via an explicit `to-create`:

```perl6
define {
  .skip-create;

  .factory: 'log-event', { .text: 'noop' };

  .factory: 'invoice', {
    .total: 0;
    .to-create: -> $inv, $eval { $billing.commit($inv); };   # overrides global skip
  };
};
```

`ORM::Factory.global-skip-create` returns the global flag (or undefined);
`ORM::Factory.reload` clears it.

## Resolution order

For each build, the hook is resolved by walking child → parent → global. The
first match wins:

1. The factory's own hook;
2. each parent factory's hook, child-first;
3. the global hook from `define`;
4. the adapter default (`instantiate` / `persist`).

For `create`, `to-create` and `skip-create` share the same chain — whichever
appears first wins, so a child `to-create` can override a parent `skip-create`
(or vice versa).

## Interaction with callbacks

The callback timeline is the same as the default path, with the hooks
substituted in:

| Strategy        | Step 1            | Step 2          | Step 3         | Step 4         |
| --------------- | ----------------- | --------------- | -------------- | -------------- |
| `build`         | `initialize-with` | `after build`   | —              | —              |
| `create`        | `initialize-with` | `after build`   | `before create`→ persistence (`to-create` / `skip-create` / adapter) → `after create` | |
| `build-stubbed` | `initialize-with` | `stub` (adapter) | `after stub` | — |
| `attributes-for`| (bypassed — returns the attribute hash directly) | | | |
