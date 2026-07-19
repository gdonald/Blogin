---
title: Dependent attributes
slug: dependent-attrs
order: 9
description: An attribute defined as a block can reference any other attribute on the factory by calling it as a method on the evaluator. The evaluator is bound to $_...
toc: true
---
An attribute defined as a block can reference any other attribute on the
factory by calling it as a method on the evaluator. The evaluator is bound
to `$_` inside the block, so the dot-syntax stays terse.

## Referencing another attribute

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
    .email: { .fname.lc ~ '@example.com' };
  };
};

ORM::Factory.build('user').email;   # 'greg@example.com'
```

`.fname` inside the `email` block reads `$_.fname` on the evaluator, which
returns the resolved value (after caching).

## Declaration order is preserved

The order you declare attributes is the order they live on the
`FactoryDefinition`. The evaluator memoises each attribute the first time
it is requested, so a downstream attribute that pulls an upstream value
sees the cached result regardless of textual order:

```perl6
define {
  .factory: 'user', {
    .fname:    'Greg';
    .lname:    'Donald';
    .email:    { .fname ~ '@example.com' };
    .nickname: { .fname.lc };
  };
};

ORM::Factory.factory-by-name('user').attributes.map(*.name).list;
# ['fname', 'lname', 'email', 'nickname']
```

## Overrides flow through

A per-call override is visible to dependent attributes too — the override
goes through the same evaluator, so the dependent block sees the override
rather than the declared default:

```perl6
ORM::Factory.build('user', :fname<Alice>).email;   # 'alice@example.com'
```

See [Overrides](/docs/ORM-Factory/overrides) for the override mechanics in detail.
