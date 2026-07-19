---
title: Class resolution
slug: class-resolution
order: 7
description: Every factory needs a class to instantiate. ORM::Factory tries to infer it from the factory name, but you can override the inference explicitly, or...
toc: true
---
Every factory needs a class to instantiate. `ORM::Factory` tries to infer it
from the factory name, but you can override the inference explicitly, or
disable it entirely.

## Inference from factory name

The factory name is camelized and looked up in `GLOBAL`. Both kebab- and
snake-case names are supported:

| factory name      | inferred class name |
| ----------------- | ------------------- |
| `'user'`          | `User`              |
| `'super-admin'`   | `SuperAdmin`        |
| `'team_lead'`     | `TeamLead`          |
| `'top-level_mix'` | `TopLevelMix`       |

If the corresponding class is present in `GLOBAL`, the factory binds to it:

```perl6
class User { has Str $.fname; }

define {
  .factory: 'user', { ; };
};

ORM::Factory.factory-by-name('user').class;        # User
ORM::Factory.factory-by-name('user').class-name;   # 'User'
```

## Explicit `:class` override

Pass `:class(...)` to bind a factory to a specific class regardless of name:

```perl6
define {
  .factory: 'super-admin', :class(Admin), { ; };
};

ORM::Factory.factory-by-name('super-admin').class;        # Admin
ORM::Factory.factory-by-name('super-admin').class-name;   # 'SuperAdmin' (still inferred)
```

The inferred `class-name` is preserved for introspection even when `:class`
is explicit.

## Disabling inference

When you do not want `ORM::Factory` to touch `GLOBAL` — for example, in a
spec that exercises only attribute capture — set the lookup toggle off
before defining:

```perl6
ORM::Factory.set-allow-class-lookup(False);

define {
  .factory: 'user', { ; };
};

ORM::Factory.factory-by-name('user').class.defined;   # False
```

The toggle defaults to `True` and is reset by `reload` only in the sense
that the registry is cleared — the toggle itself is sticky, so flip it back
on (`set-allow-class-lookup(True)`) when you are done.

## Missing classes

If the inferred class cannot be found and you ask for it through
`lookup-class`, the factory raises `X::ORM::Factory::UnknownClass` with a
message that names the factory and the class it was looking for. The error
text points to the three available fixes: declare the class, pass `:class`
explicitly, or disable lookup.
