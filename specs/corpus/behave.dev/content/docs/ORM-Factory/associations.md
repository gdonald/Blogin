---
title: Associations
slug: associations
order: 12
description: An *association* binds an attribute on one factory to an instance built by another. ORM::Factory resolves associations lazily at build time, so the...
toc: true
---
An *association* binds an attribute on one factory to an instance built by
another. `ORM::Factory` resolves associations lazily at build time, so the
strategy you use on the parent (`build`, `create`, `build-stubbed`) cascades to
the association by default.

## Implicit associations

When the attribute name matches a registered factory name (or alias), a bare
declaration is enough — the attribute resolves to a fresh build of that
factory:

```perl6
define {
  .factory: 'user', :aliases<author>, {
    .fname: 'Greg';
  };

  .factory: 'post', {
    .title: 'Hello';
    .author;     # implicit: 'author' aliases 'user'
  };
};

ORM::Factory.build('post').author.fname;   # 'Greg'
```

Implicit associations are excluded from `attributes-for` output — they
describe an object, not a column.

## Explicit `association`

When the attribute name differs from the target factory, declare the
association explicitly with the `:factory` adverb:

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
  };

  .factory: 'post', {
    .title: 'Hello';
    .association: 'author', :factory<user>;
  };
};
```

You can also pass attribute overrides and variants in the same call. Any
named arguments other than `:factory` and `:strategy` are forwarded as
overrides to the target factory; positional strings after the name are
applied as variants:

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
    .role:  'member';
    .variant: 'admin', { .role: 'admin' };
  };

  .factory: 'post', {
    .association: 'author', 'admin', :factory<user>, :fname<Alice>;
  };
};

ORM::Factory.build('post').author.role;    # 'admin'   (variant)
ORM::Factory.build('post').author.fname;   # 'Alice'   (override)
```

## Strategy cascade and `:strategy` override

By default, associations follow the parent's build strategy — `build('post')`
builds the author, `create('post')` creates it, and `build-stubbed('post')`
stubs it. Override per association with `:strategy`:

```perl6
define {
  .factory: 'user', { .fname: 'Greg' };

  .factory: 'post', {
    .title: 'Hello';
    .association: 'author', :factory<user>, :strategy<build>;
  };
};

ORM::Factory.create('post').saved;          # True
ORM::Factory.create('post').author.saved;   # False — author was built, not created
```

Valid strategies: `build`, `create`, `build-stubbed`, `attributes-for`.

## Inline association block

For an association whose target depends on other attributes — including
transients — use a dynamic block. The block runs against the evaluator and
returns the built object:

```perl6
define {
  .factory: 'user', { .fname: 'Greg' };

  .factory: 'post', {
    .transient: {
      .author-name: 'Inline';
    };
    .author: { ORM::Factory.build('user', :fname(.author-name)) };
  };
};

ORM::Factory.build('post').author.fname;                    # 'Inline'
ORM::Factory.build('post', :author-name<Alice>).author.fname;   # 'Alice'
```

## `has_many`-style collections

A `has_many` collection is a transient count plus an `after build` callback
that pushes children:

```perl6
define {
  .factory: 'user', { .fname: 'Greg' };

  .factory: 'post', {
    .title: 'Hello';

    .transient: {
      .comments-count: 0;
    };

    .after: 'build', -> $post, $eval {
      for ^$eval.comments-count -> $i {
        $post.comments.push: ORM::Factory.build('comment', :post($post));
      }
    };
  };
};

ORM::Factory.build('post', :comments-count(3)).comments.elems;   # 3
```

See [Callbacks](/docs/ORM-Factory/callbacks) for the full list of hook events.

## Polymorphic-style associations

A polymorphic association is just an explicit target factory, so the
`:factory` adverb handles it. The attribute type on the model holds
whichever instance the chosen factory produces:

```perl6
define {
  .factory: 'post', {
    .title: 'Hello';
  };

  .factory: 'comment', {
    .body: 'Nice';
    .association: 'commentable', :factory<post>;
  };
};

ORM::Factory.build('comment').commentable;   # a Post instance
```

## Error reporting

- `X::ORM::Factory::MissingAssociation` is raised when an association targets
  a factory that is not registered.
- `X::ORM::Factory::CyclicAssociation` is raised when building a factory
  reaches itself through the association chain.

Both exceptions carry the offending factory name and the build chain, so
fixing the definition usually means renaming the target or breaking the cycle
with an inline block.
