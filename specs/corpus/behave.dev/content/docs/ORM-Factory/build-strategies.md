---
title: Build strategies
slug: build-strategies
order: 15
description: ORM::Factory ships four strategies for turning a factory definition into a result. They all share the same surface — (Str:D $name, *@variants,...
toc: true
---
`ORM::Factory` ships four strategies for turning a factory definition into a
result. They all share the same surface — `(Str:D $name, *@variants, *%overrides)`
— and dispatch through a small `Strategy` role.

| Method            | What it returns                                  | Persists? |
|-------------------|--------------------------------------------------|-----------|
| `build`           | An unsaved instance.                             | No        |
| `create`          | A persisted instance.                            | Yes       |
| `attributes-for`  | A `Hash` of resolved attributes.                 | No        |
| `build-stubbed`   | An adapter-stubbed instance (no DB access).      | No        |

```perl6
my $user  = ORM::Factory.build('user');                  # unsaved
my $saved = ORM::Factory.create('user');                 # persisted
my %h     = ORM::Factory.attributes-for('user');         # attribute hash
my $stub  = ORM::Factory.build-stubbed('user');          # stubbed
```

## Variants

Variants are positional after the factory name:

```perl6
ORM::Factory.create('user', 'admin');
ORM::Factory.create('user', 'admin', 'active');     # left-to-right, later wins
```

An unknown variant raises `X::ORM::Factory::UnknownVariant`.

## Per-call overrides

Override any attribute (static or dynamic) by passing a named arg:

```perl6
ORM::Factory.build('user', :fname<Alice>);
ORM::Factory.build('user', 'admin', :email<a@b.c>);
```

A `Callable` override replaces the attribute with a dynamic block evaluated
in the evaluator context — so dependent attributes see the override:

```perl6
ORM::Factory.build('user', :fname<Alice>, :nick({ .fname.uc }));   # 'ALICE'
ORM::Factory.build('user', :fname<Alice>).email;                   # 'alice@example.com'
```

`attributes-for` and `build-stubbed` honour the same overrides.

## Collections

```perl6
ORM::Factory.build-list('user', 3);                      # 3 instances
ORM::Factory.build-list('user', 3, 'admin');             # variant applied to all
ORM::Factory.build-list('user', 3, :fname<X>);           # overrides applied to all
ORM::Factory.build-list('user', 3, -> $u, $i {           # post-build block per instance
  $u.fname = "User$i";
});
```

The variants form, the overrides form, and the post-build block can be
combined freely; the block is recognised by its `Callable` type at the tail
of the positional args.

The same shape works for `create-list`, `build-stubbed-list`, and
`attributes-for-list`. The `build-pair` and `create-pair` shortcuts are
fixed-count `*-list` variants returning two instances.

## Strategy role (`ORM::Factory::Strategy`)

The public methods dispatch through a small role:

```perl6
role ORM::Factory::Strategy {
  has $.persistence;
  method to-sym(--> Str) { ... }
  method result(Evaluator $eval) { ... }
  method association(Str:D $name, @variants, %overrides) { ... }
}
```

The four shipped strategies are `BuildStrategy`, `CreateStrategy`,
`AttributesForStrategy`, and `BuildStubbedStrategy`. Register your own with
`ORM::Factory.register-strategy` — see [Custom strategies](/docs/ORM-Factory/custom-strategies).
