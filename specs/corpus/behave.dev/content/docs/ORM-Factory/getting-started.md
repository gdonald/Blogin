---
title: Getting started
slug: getting-started
order: 2
description: This page walks you through defining your first factory, reading it back from the registry, and resetting state between tests.
toc: true
---
This page walks you through defining your first factory, reading it back from
the registry, and resetting state between tests.

## Install

```
zef install --/test ORM::Factory
```

## Defining a factory

`define` opens a definition block. Inside, the topic (`$_`) is
bound to a `DefinitionBuilder`, so DSL calls use the leading-dot shorthand:

```perl6
use ORM::Factory;

define {
  .factory: 'user', {
    .fname: 'Greg';
    .lname: 'Donald';
    .email: { 'user@example.com' };
  };
};
```

A *static* attribute takes any non-`Callable` value: `.fname: 'Greg'`. A
*dynamic* attribute takes a single `Callable` and is evaluated lazily:
`.email: { 'user@example.com' }`.

## Class inference and overrides

`ORM::Factory` camelizes the factory name (`'user'` → `User`, `'super-admin'` →
`SuperAdmin`, `'team_lead'` → `TeamLead`) and looks the result up in `GLOBAL`.
A top-level `class User { … }` in any compunit will be found.

When the camelized name doesn't match your class — or the class lives in a
namespace — pass it explicitly:

```perl6
define {
  .factory: 'super-admin', :class(MyApp::Admin), {
    .role: 'admin';
  };
};
```

To turn off name-based inference entirely (e.g. you only want explicit
classes):

```perl6
ORM::Factory.set-allow-class-lookup(False);
```

`lookup-class` on a `FactoryDefinition` raises
`X::ORM::Factory::UnknownClass` if the class can't be resolved and no
override was supplied.

> behave EVALs each spec file. `our class Foo { … }` declared during EVAL
> does **not** land in `GLOBAL` the way a normal compunit does, so name
> inference may miss it. In behave specs, either bind explicitly
> (`BEGIN GLOBAL::<Foo> := Foo;`) or pass `:class(Foo)` per factory.

## Aliases

A factory can be registered under additional names:

```perl6
define {
  .factory: 'user', :aliases<author commenter>, {
    .fname: 'Greg';
  };
};

say ORM::Factory.factory-by-name('author').name;   # 'user'
say ORM::Factory.aliases<commenter>;               # 'user'
```

Aliases collide with both factory names and other aliases — the
duplicate-registration guard raises `X::ORM::Factory::DuplicateAlias` if you
try to claim a name that's already in use.

## Reading the registry

Definitions land in a process-global registry keyed by name:

```perl6
my %factories = ORM::Factory.factories;        # all definitions
my $user      = ORM::Factory.factory-by-name('user');

say $user.name;                                 # 'user'
say $user.attributes.elems;                     # 3
say $user.attributes[0].name;                   # 'fname'
say $user.attributes[0].value;                  # 'Greg'
```

Looking up an unregistered name raises `X::ORM::Factory::UnknownFactory`.

## Variants

A *variant* is an alternate set of overrides scoped to one factory. Define
one with `.variant`, apply it inside the same factory body with a bare
leading-dot call:

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';

    .variant: 'admin', {
      .role: 'admin';
    };

    .admin;
  };
};
```

The `.admin` call resolves against the registered variant name first. If the
name isn't a registered variant, it falls back to capturing an attribute
named `admin`.

## Names that collide with the DSL

DSL method names — `factory`, `variant`, `transient`, `association`,
`add-attribute`, `before`, `after`, `initialize-with`, `to-create`, `modify`,
`skip-create` — cannot be captured by the FALLBACK shortcut. Use the explicit
`add-attribute` escape hatch:

```perl6
.add-attribute: 'factory', 'acme';   # attribute literally named "factory"
.add-attribute: 'block',   -> { 42 };  # a Callable literal as a static value
```

`add-attribute` skips the FALLBACK indirection and stores exactly what you
pass: a `Callable` arg is treated as dynamic, anything else as static, with
no chance of colliding with a method name on the builder.

## Resetting between tests

`ORM::Factory.reload` clears the registry. Run it before each test to keep
suites from leaking definitions:

```perl6
use BDD::Behave;
use ORM::Factory;

describe 'my feature', {
  before-each {
    ORM::Factory.reload;
  }

  # …
};
```

For `prove6`, just call `ORM::Factory.reload` at the top of each test block.
See [Tests](/docs/ORM-Factory/tests) for the full convention.

## Diagnostics

The library raises typed exceptions under `X::ORM::Factory::*`:

| Exception                            | Trigger                                      |
|--------------------------------------|----------------------------------------------|
| `X::ORM::Factory::UnknownFactory`    | `factory-by-name` on an unregistered name.   |
| `X::ORM::Factory::DuplicateFactory`  | Two `.factory 'x', …` calls with the same name. |
| `X::ORM::Factory::DuplicateVariant`  | Two `.variant 'x', …` calls in the same factory. |
| `X::ORM::Factory::UsageError`        | Argument shape the DSL can't make sense of (e.g. a variant applied with arguments). |
