---
title: Bare-name DSL
slug: bare-name-dsl
order: 4
description: ORM::Factory::DSL is an opt-in module that defines factories without the leading-dot method-call syntax. It is plain Raku (exported subs over a dynamic...
toc: true
---
`ORM::Factory::DSL` is an opt-in module that defines factories without the
leading-dot method-call syntax. It is plain Raku (exported subs over a dynamic
builder), so it carries no slang, no macro, no precompilation risk, and full
editor tooling. The canonical dotted form is unchanged and remains the default.

Use this module **instead of** `ORM::Factory`; it re-exports the build and query
helpers, so it is self-contained.

```perl6
use ORM::Factory::DSL;
```

## The three forms

All three define the same factory and dispatch to the same builders. Pick per
taste; they can be mixed in one block.

Canonical dotted form (`use ORM::Factory`):

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
    .email: { generate('email') };
    .variant: 'admin', { .role: 'admin' };
  };
};
```

Bare-name keyword form (`use ORM::Factory::DSL`):

```perl6
define {
  factory 'user', {
    attr 'fname', 'Greg';
    attr 'email', { generate('email') };
    variant 'admin', { attr 'role', 'admin' };
  };
};
```

Colon-pair form (also `ORM::Factory::DSL`):

```perl6
define {
  factory 'user', {
    attrs(
      :fname<Greg>,
      :email({ generate('email') }),
    );
  };
};
```

## Keywords

Every DSL keyword is available as a bare sub: `factory`, `variant`, `transient`,
`sequence`, `association`, `before`, `after`, `callback`, `to-create`,
`initialize-with`, `skip-create`, `variants-for-enum`, plus `define` and
`modify`. They dispatch to the active builder, so nested blocks (factory,
variant, transient) retarget correctly:

```perl6
define {
  sequence 'email', -> $n { "user{$n}\@example.com" };

  factory 'user', {
    attr 'fname', 'Greg';
    attr 'email', { generate('email') };

    variant 'admin', { attr 'role', 'admin' };

    after 'build', -> $user, $ { $user.role //= 'member' };
  };

  factory 'post', {
    attr 'title', 'Hello';
    association 'author', :factory<user>;
  };
};

build('user', 'admin').role;     # 'admin'
build('post').author.fname;      # 'Greg'
```

## Attributes: `attr` vs `attrs`

`attr NAME, VALUE` is the explicit setter. A `Callable` value is a dynamic
(lazily evaluated) attribute, the same as the dotted `.name: { ... }` form.
`attr` preserves declaration order, so it is the form to use for a dependent
chain that must read in a fixed order:

```perl6
factory 'user', {
  attr 'fname', 'Greg';
  attr 'slug',  { $_.fname.lc };   # reads a prior attribute
};
```

`attrs(:NAME<VALUE>, ...)` takes a colon-pair list. A `Callable` value is again
a dynamic attribute. Because the pairs arrive as named arguments, their order is
**not** preserved across the list; use sequential `attr` calls when order
matters.

```perl6
factory 'user', {
  attrs(
    :fname<Greg>,
    :lname<Donald>,
    :email({ generate('email') }),
  );
};
```

A dynamic value must be a flexible-arity `{ ... }` block, not a strict
`-> { ... }`, because the evaluator passes the block an argument (the same rule
as the dotted form).

## Building

The build and query helpers are re-exported, so no separate `use ORM::Factory`
is needed: `build`, `create`, `build-stubbed`, `attributes-for`, the `-list` and
`-pair` variants, `generate`, `generate-list`, and `reload`.
