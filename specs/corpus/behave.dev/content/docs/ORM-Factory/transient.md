---
title: Transient attributes
slug: transient
order: 10
description: A *transient* attribute is visible inside the factory (to dynamic blocks, overrides, and callbacks) but is not passed to the model constructor and is not...
toc: true
---
A *transient* attribute is visible inside the factory (to dynamic blocks,
overrides, and callbacks) but is **not** passed to the model constructor and
is **not** returned by `attributes-for`. Use them for "knobs" — flags or
inputs that shape how the real attributes get computed, without polluting
the model itself.

## Declaring

Wrap the declarations in a `.transient` block:

```perl6
define {
  .factory: 'greeting', {
    .transient: {
      .upcase: False;
      .who:    'World';
    };

    .text: { .upcase ?? "HELLO, {.who.uc}" !! "Hello, {.who}" };
  };
};
```

Inside `.transient`, every attribute capture (static, dynamic, or
`add-attribute`) is marked transient. Attributes outside the block are
normal persisted attributes.

## Visibility

Transient values are first-class to the evaluator. Dynamic attributes and
callbacks see them via the leading-dot shorthand:

```perl6
ORM::Factory.build('greeting').text;                       # 'Hello, World'
ORM::Factory.build('greeting', :who<Greg>).text;           # 'Hello, Greg'
ORM::Factory.build('greeting', :upcase(True)).text;        # 'HELLO, WORLD'
```

Overrides for a transient flow through to anything that depends on it.

## Excluded from the model and `attributes-for`

The model constructor never sees transient values — `Greeting.new` above is
called with only `text => ...`, never `upcase` or `who`. Likewise:

```perl6
ORM::Factory.attributes-for('greeting');                   # { text => 'Hello, World' }
```

`attributes-for` is the canonical "what does this factory produce as data"
view, so it follows the same exclusion.
