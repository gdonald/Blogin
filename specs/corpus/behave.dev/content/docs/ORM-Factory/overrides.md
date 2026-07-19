---
title: Overrides
slug: overrides
order: 8
description: Every build method — build, create, attributes-for, build-stubbed, and their -list / -pair cousins — accepts named arguments that override attributes for...
toc: true
---
Every build method — `build`, `create`, `attributes-for`, `build-stubbed`,
and their `-list` / `-pair` cousins — accepts named arguments that override
attributes for that call only. The original definition is untouched.

## Static override of a static attribute

```perl6
define {
  .factory: 'profile', {
    .fname: 'Greg';
    .nick:  'gd';
  };
};

ORM::Factory.build('profile', :fname<Alice>).fname;   # 'Alice'
ORM::Factory.build('profile', :fname<Alice>).nick;    # 'gd'  (untouched)
```

## Static override of a dynamic attribute

A static override replaces a block defined on the factory with the literal
value:

```perl6
define {
  .factory: 'profile', {
    .fname: 'Greg';
    .email: { .fname.lc ~ '@example.com' };
  };
};

ORM::Factory.build('profile', :email<x@y.z>).email;   # 'x@y.z'
```

## Block override

Pass a callable to override with a fresh block. The block runs against the
evaluator, so it can read other attributes via `$_`:

```perl6
ORM::Factory.build('profile', :fname<Alice>, :nick({ .fname.uc })).nick;
# 'ALICE'
```

Overrides apply uniformly across all four build strategies:

```perl6
ORM::Factory.attributes-for('profile', :fname<Carol>)<fname>;   # 'Carol'
ORM::Factory.create('profile',         :fname<Carol>).fname;    # 'Carol'
ORM::Factory.build-stubbed('profile',  :fname<Carol>).fname;    # 'Carol'
```

## Dependent attributes see the override

The override goes through the evaluator before any dependent attribute is
resolved, so a block that reads `.fname` sees the overridden value:

```perl6
ORM::Factory.build('profile', :fname<Alice>).email;
# 'alice@example.com'
```
