---
title: Inheritance
slug: inheritance
order: 13
description: A factory can derive from another factory, picking up its attributes, transients, and variants while adding or overriding pieces of its own. The parent's...
toc: true
---
A factory can derive from another factory, picking up its attributes,
transients, and variants while adding or overriding pieces of its own. The
parent's attribute list is resolved at build time, so anything you change on
the parent (via `modify`, see below) propagates to children automatically.

## Nested factory definition

Declaring a `factory` inside another factory's block makes the inner factory
a child of the outer one. The child's `parent-name` is set implicitly:

```perl6
define {
  .factory: 'person', {
    .fname: 'Greg';
    .email: 'greg@example.com';

    .factory: 'admin', {
      .role: 'admin';
    };
  };
};

ORM::Factory.build('admin').fname;   # 'Greg'
ORM::Factory.build('admin').email;   # 'greg@example.com'
ORM::Factory.build('admin').role;    # 'admin'
```

The child factory is registered at the top level — `factory-by-name('admin')`
works just like any other factory.

## Explicit `:parent` option

When the parent already exists at the top level, use `:parent('name')`
instead of nesting:

```perl6
define {
  .factory: 'person', {
    .fname: 'Greg';
  };

  .factory: 'manager', :parent('person'), {
    .role: 'manager';
  };
};
```

The parent must already be defined — either earlier in the same `.define`
block or in a previous one. Unknown parents raise
`X::ORM::Factory::UnknownFactory`.

## Overriding inherited attributes

The child wins on any attribute it re-declares:

```perl6
define {
  .factory: 'person', {
    .fname: 'Greg';
  };

  .factory: 'admin-person', :parent('person'), {
    .fname: 'Admin Greg';   # overrides
  };
};

ORM::Factory.build('admin-person').fname;   # 'Admin Greg'
```

## Multi-level chains

Inheritance chains are resolved root → leaf, with each level overriding the
last:

```perl6
define {
  .factory: 'person',  { .fname: 'Greg' };
  .factory: 'manager', :parent('person'),  { .role: 'manager' };
  .factory: 'cto',     :parent('manager'), { .flag: True };
};

ORM::Factory.build('cto').fname;   # 'Greg'    (grandparent)
ORM::Factory.build('cto').role;    # 'manager' (parent)
ORM::Factory.build('cto').flag;    # True      (own)
```

## Class resolution

A child factory's class defaults to the parent's class, not to the
camelized child name. Pass `:class(...)` explicitly to override:

```perl6
define {
  .factory: 'person', { .fname: 'Greg' };

  # 'admin-person'.lookup-class is Person (inherited from parent),
  # not Admin-Person (which doesn't exist).
  .factory: 'admin-person', :parent('person'), { .role: 'admin' };

  # explicit :class wins.
  .factory: 'worker-person', :parent('person'), :class(Worker), {
    .title: 'Engineer';
  };
};
```

## Transient inheritance

A transient declared on the parent is available to the child's dynamic
attributes and overrides, exactly as if it had been declared on the child:

```perl6
define {
  .factory: 'person', {
    .transient: {
      .upcase: False;
    };
    .fname: { .upcase ?? 'GREG' !! 'Greg' };
  };

  .factory: 'admin-person', :parent('person'), { ; };
};

ORM::Factory.build('admin-person').fname;                  # 'Greg'
ORM::Factory.build('admin-person', :upcase(True)).fname;   # 'GREG'
```

`attributes-for` on the child still excludes the inherited transient — the
transient flag travels with the attribute.

## Variant inheritance

Variants defined on a parent are visible to its descendants. A child can
apply them by bare name:

```perl6
define {
  .factory: 'person', {
    .fname: 'Greg';
    .variant: 'admin', {
      .role: 'admin';
    };
  };

  .factory: 'admin-person', :parent('person'), {
    .admin;   # applies the parent's variant
  };
};

ORM::Factory.build('admin-person').role;   # 'admin'
```

## `modify` — changing an already-defined factory

`ORM::Factory.modify` lets you re-open an existing factory and replace
attributes (or add new ones). Anything you don't touch is left alone:

```perl6
define {
  .factory: 'person', {
    .fname: 'Greg';
    .email: 'greg@example.com';
  };
};

ORM::Factory.modify: {
  .factory: 'person', {
    .fname: 'Modified';   # overrides
  };
};

ORM::Factory.build('person').fname;   # 'Modified'
ORM::Factory.build('person').email;   # 'greg@example.com'  (untouched)
```

Modifying an unknown factory raises `X::ORM::Factory::UnknownFactory`.

### Modifications propagate through inheritance

Because the child resolves its attribute list at build time, a `modify` on
the parent is automatically visible to descendants — unless the child has
overridden the attribute itself:

```perl6
define {
  .factory: 'person', { .fname: 'Greg' };

  .factory: 'admin-person', :parent('person'), {
    .role: 'admin';
  };
};

ORM::Factory.modify: {
  .factory: 'person', { .fname: 'Patched' };
};

ORM::Factory.build('admin-person').fname;   # 'Patched' (parent's modify)
ORM::Factory.build('admin-person').role;    # 'admin'   (child's own attribute)
```
