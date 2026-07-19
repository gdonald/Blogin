---
title: Overview
slug: overview
order: 1
description: The latest version of this documentation lives at https://gdonald.github.io/ORM-Factory/.
toc: true
---
The latest version of this documentation lives at [https://gdonald.github.io/ORM-Factory/](https://gdonald.github.io/ORM-Factory/).

The homepage for ORM::Factory is [https://github.com/gdonald/ORM-Factory](https://github.com/gdonald/ORM-Factory).

## Synopsis

`ORM::Factory` is a definition DSL for building test objects (with or
without persistence) so your specs stay declarative.

It is ORM-agnostic at its core. With
[`ORM::ActiveRecord`](https://github.com/gdonald/ORM-ActiveRecord) installed
and auto-detected, `create` persists through the model's `save-bang` with
validations, callbacks, and timestamps intact. With no ORM loaded, factories
still build plain objects, and the `to-create` / `initialize-with` hooks let
you target any persistence layer.

## Example usage

```perl6
use ORM::Factory;

define {
  .factory: 'user', {
    .fname: 'Greg';
    .lname: 'Donald';
    .email: { 'user@example.com' };

    .variant: 'admin', {
      .role: 'admin';
    };
  };
};
```

## Install

`ORM::Factory` can be installed using the [zef](https://github.com/ugexe/zef)
module installation tool:

```
zef install --/test ORM::Factory
```

`--/test` is suggested because the full suite exercises every supported
adapter (PostgreSQL, MySQL, SQLite). The library itself has no runtime
dependencies; `ORM::ActiveRecord` is a `test-depends` and is auto-detected at
runtime when installed.

## Where to go next

- [Getting started](/docs/ORM-Factory/getting-started) — define and use your first factory.
- [DSL design](/docs/ORM-Factory/dsl-design) — the rationale behind the topic-`$_` + `FALLBACK` DSL surface.
- [Tests](/docs/ORM-Factory/tests) — running the suite, the `t/` ↔ `specs/` mirror convention, and the `db/` vs unit layout.
