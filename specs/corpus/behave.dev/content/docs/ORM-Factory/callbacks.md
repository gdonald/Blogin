---
title: Callbacks
slug: callbacks
order: 20
description: Callbacks hook into the build lifecycle so a factory can mutate its instance or trigger side-effects at well-defined points. The built-in events are after...
toc: true
---
Callbacks hook into the build lifecycle so a factory can mutate its instance
or trigger side-effects at well-defined points. The built-in events are
`after build`, `before create`, `after create`, and `after stub`; custom
names can be invoked by hand from other callbacks or custom build
strategies.

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
    .after: 'build', -> $u, $eval { $u.fname = $u.fname.uc };
  };
};

ORM::Factory.build('user').fname;   # 'GREG'
```

## Built-in events

| Event           | Fired during                       | Phase    |
| --------------- | ---------------------------------- | -------- |
| `after build`   | `build`, `create`                  | `after`  |
| `before create` | `create` (before persistence)      | `before` |
| `after create`  | `create` (after persistence)       | `after`  |
| `after stub`    | `build-stubbed`                    | `after`  |

`attributes-for` runs no callbacks — it returns the attribute hash without
ever instantiating the model.

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';

    .before: 'create', -> $u, $eval { $u.fname = "[$u.fname()]" };
    .after:  'create', -> $u, $eval { say "persisted user: $u.fname()" };
    .after:  'stub',   -> $u, $eval { $u.fname = '<stubbed>'         };
  };
};
```

## Callback signature

The block can take zero, one, or two arguments:

- 0 — for fire-and-forget side effects
- 1 — receives the instance under construction
- 2 — receives the instance and the [evaluator](/docs/ORM-Factory/transient), so the
  callback can read any persisted or transient attribute (including overrides)

```perl6
.after: 'build', -> $u, $eval {
  $u.greeting = "hello, {$eval.salute}";   # transient via evaluator
};
```

## Multiple callbacks for one event

A factory may declare several callbacks for the same event. They fire in
**declaration order**:

```perl6
.factory: 'user', {
  .after: 'build', -> $u, $e { $u.events.push: 'one'   };
  .after: 'build', -> $u, $e { $u.events.push: 'two'   };
  .after: 'build', -> $u, $e { $u.events.push: 'three' };
};
# events: ['one', 'two', 'three']
```

## Inheritance

Parent callbacks run **before** child callbacks. Each factory contributes its
own callbacks at its position in the chain:

```perl6
define {
  .factory: 'user', {
    .after: 'build', -> $u, $e { $u.events.push: 'parent' };

    .factory: 'admin', {
      .role: 'admin';
      .after: 'build', -> $u, $e { $u.events.push: 'child' };
    };
  };
};

ORM::Factory.build('admin').events;   # ['parent', 'child']
```

## Variants

A [variant](/docs/ORM-Factory/variants) can register its own callbacks. When the variant is
applied, its callbacks are appended after the factory's own:

```perl6
define {
  .factory: 'user', {
    .after: 'build', -> $u, $e { $u.events.push: 'base' };

    .variant: 'noisy', {
      .after: 'build', -> $u, $e { $u.events.push: 'noisy' };
    };
  };
};

ORM::Factory.build('user').events;          # ['base']
ORM::Factory.build('user', 'noisy').events; # ['base', 'noisy']
```

## Global callbacks

A `before` / `after` at the top of a `define` block — without a
surrounding `factory` — applies to **every** factory. Global callbacks fire
**before** any factory-specific callbacks for the same event:

```perl6
define {
  .after: 'build', -> $i, $e { $i.events.push: 'global' };

  .factory: 'user', {
    .fname: 'Greg';
    .after: 'build', -> $u, $e { $u.events.push: 'user' };
  };
};

ORM::Factory.build('user').events;   # ['global', 'user']
```

`ORM::Factory.global-callbacks` returns the list of registered globals.
`ORM::Factory.reload` clears them along with everything else.

## Custom callbacks

`callback 'name'` registers a callback under an arbitrary event name. Custom
callbacks do not fire on any built-in event — they only fire when something
asks for them explicitly via `evaluator.run-callbacks('name')`. The natural
caller is a custom [build strategy](/docs/ORM-Factory/strategy), but built-in callbacks can
also chain to a custom name:

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
    .callback: 'shouted', -> $u, $e { $u.fname = $u.fname.uc };
    .after: 'build', -> $u, $e { $e.run-callbacks('shouted') };
  };
};

ORM::Factory.build('user').fname;   # 'GREG'
```

## `has_many`-style collections

A [transient attribute](/docs/ORM-Factory/transient) plus an `after build` callback gives
a count knob and a side-effect that builds the children:

```perl6
define {
  .factory: 'user', { .fname: 'Greg' };

  .factory: 'post', {
    .title: 'Hello';

    .transient: {
      .comments-count: 0;
    };

    .after: 'build', -> $post, $eval {
      for ^$eval.comments-count -> $i {
        $post.comments.push: ORM::Factory.build('comment', :post($post));
      }
    };
  };
};

ORM::Factory.build('post', :comments-count(3)).comments.elems;   # 3
```

The transient `comments-count` is excluded from `attributes-for` and never
reaches the model constructor — only the callback sees it via the evaluator.

## Reset between tests

`ORM::Factory.reload` clears every factory, alias, sequence, global variant,
**and** global callback. Per-test reset is identical to the rest of the
library:

```perl6
before-each {
  ORM::Factory.reload;
  ORM::Factory.reset-persistence;
};
```
