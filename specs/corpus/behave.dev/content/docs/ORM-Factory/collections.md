---
title: Collections
slug: collections
order: 17
description: Each of the core build strategies has a -list form that produces N instances, plus a -pair shortcut for two. The same variant, override, and block-form...
toc: true
---
Each of the core build strategies has a `-list` form that produces N
instances, plus a `-pair` shortcut for two. The same variant, override, and
block-form arguments accepted by the single-build methods carry over to the
collection methods.

## `build-list` / `create-list`

```perl6
ORM::Factory.build-list('item', 3);            # 3 unsaved items
ORM::Factory.create-list('item', 2);           # 2 persisted items
```

Each call evaluates the factory once per instance, so dynamic attributes
(including sequences) advance just as they would with N separate calls.

### Per-call variants and overrides

Pass variants and named overrides after the count — they apply to every
instance:

```perl6
ORM::Factory.build-list('item', 2, 'admin');                # both admins
ORM::Factory.build-list('item', 2, :label<X>)[1].label;     # 'X'
```

### Block form

Pass a final callable to receive each built instance with its zero-based
index:

```perl6
my @items = ORM::Factory.build-list('item', 3, -> $it, $i {
  $it.index = $i;
});

@items.map(*.index).List;   # (0, 1, 2)
```

## `build-stubbed-list` / `attributes-for-list`

Identical shape, different strategy. `attributes-for-list` returns an
`Array[Hash]`; `build-stubbed-list` returns stubbed instances that do not
touch the database.

```perl6
ORM::Factory.attributes-for-list('item', 3);
ORM::Factory.build-stubbed-list('item', 2);
```

## Pair shortcuts

`build-pair` and `create-pair` are the count-of-two convenience aliases.
They accept the same variants, overrides, and block form as the `-list`
methods:

```perl6
ORM::Factory.build-pair('item');
ORM::Factory.create-pair('item');

ORM::Factory.build-pair('item', 'admin', :label<X>);
ORM::Factory.build-pair('item', -> $it, $i { $it.index = $i });
```
