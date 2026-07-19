---
title: Aliases
slug: aliases
order: 6
description: A factory can register one or more aliases — alternative names that look up the same definition. Aliases also drive the implicit-association lookup (see...
toc: true
---
A factory can register one or more **aliases** — alternative names that look
up the same definition. Aliases also drive the implicit-association lookup
(see [Associations](/docs/ORM-Factory/associations)), so a `:post` factory whose `:author`
column references a `:user` factory is one alias away from working without
an explicit declaration.

## Declaring aliases

Pass an `:aliases` named argument when defining a factory. It accepts either
a single string or a list:

```perl6
define {
  .factory: 'user', :aliases<author commenter>, {
    .fname: 'Greg';
  };
};

ORM::Factory.factory-by-name('author').name;     # 'user'
ORM::Factory.factory-by-name('commenter').name;  # 'user'
ORM::Factory.aliases<author>;                    # 'user'
```

`factory-by-name` returns the same `FactoryDefinition` for the canonical name
and every alias, so any downstream code that looks up the factory sees the
same record.

## Collision detection

Aliases share a namespace with factory names. Any collision raises
`X::ORM::Factory::DuplicateAlias` immediately:

```perl6
# alias would shadow an existing factory
define {
  .factory: 'user',  { ; };
  .factory: 'other', :aliases<user>, { ; };   # raises
};

# two factories cannot claim the same alias
define {
  .factory: 'user',  :aliases<author>, { ; };
  .factory: 'other', :aliases<author>, { ; }; # raises
};

# a new factory cannot reuse an existing alias
define {
  .factory: 'user', :aliases<author>, { ; };
};
define {
  .factory: 'author', { ; };   # raises
};
```

## Reload

`ORM::Factory.reload` clears the alias map along with the registry. Looking
up an alias after a reload raises `X::ORM::Factory::UnknownFactory`.
