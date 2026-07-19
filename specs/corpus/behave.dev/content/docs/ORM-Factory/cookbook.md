---
title: Cookbook
slug: cookbook
order: 27
description: Common recipes for ORM::Factory, each self-contained.
toc: true
---
Common recipes for `ORM::Factory`, each self-contained.

## Sequences and unique attributes

Sequences keep unique-violating attributes unique across builds without
hard-coding values:

```perl6
define {
  .sequence: 'email', -> $n { "user{$n}\@example.com" };

  .factory: 'user', {
    .fname: 'Greg';
    .email: { ORM::Factory.generate('email') };
  };
};

ORM::Factory.build('user').email;   # user1@example.com
ORM::Factory.build('user').email;   # user2@example.com
```

For per-factory isolation, declare the sequence inline:

```perl6
.factory: 'user', {
  .sequence: 'serial';
  .email: { "user{$.serial}\@example.com" };
};
```

## Variants and variant composition

```perl6
define {
  .factory: 'user', {
    .fname: 'Greg';
    .role:  'user';

    .variant: 'admin',  { .role: 'admin' };
    .variant: 'active', { .active: True };
    .variant: 'super-admin', { admin; active };   # composes other variants
  };
};

ORM::Factory.create('user', 'admin', 'active');   # apply at build time
ORM::Factory.create('user', 'super-admin');       # one variant pulling two more
```

## Associations

Implicit (attribute name matches a factory):

```perl6
define {
  .factory: 'user', { .fname: 'Greg' };
  .factory: 'post', { .title: 'Hi'; .user };       # bare reference
};
```

Explicit, with overrides and variants:

```perl6
.factory: 'post', {
  .title: 'Hi';
  .association: 'author', :factory<user>, 'admin', :fname<Pat>;
};
```

`has_many` via callback + transient count:

```perl6
.factory: 'user', {
  .fname: 'Greg';

  .transient: {
    .post-count: 0;
  };

  .after: 'create', -> $u, $e {
    ORM::Factory.create-list('post', $e.post-count, :author($u))
      if $e.post-count > 0;
  };
};

ORM::Factory.create('user', :post-count(3));
```

## Transient attributes and the evaluator

Transient attributes are visible to dynamic attributes and callbacks but never
make it into the persisted attribute hash:

```perl6
.factory: 'user', {
  .transient: {
    .password: 'plaintext';
  };

  .password-digest: { Digest.sha1($.password) };

  .after: 'build', -> $u, $e {
    $u.session-token = "token-{$e.password}";
  };
};
```

## Custom strategies, `to-create`, and `initialize-with`

Per-factory persistence override:

```perl6
.factory: 'audit-event', {
  .name: 'view';

  .to-create: -> $event, $eval {
    AuditLog.append($event);            # writes to a flat file, not the DB
  };
};
```

A registered custom strategy:

```perl6
class JsonStrategy does ORM::Factory::Strategy {
  method to-sym { 'json' }
  method result($eval) { to-json($eval.attributes-hash) }
  method association($name, @v, %o) { ORM::Factory.build($name, |@v, |%o) }
}

ORM::Factory.register-strategy('json', JsonStrategy);
ORM::Factory.json('user');                                  # returns a JSON string
```

## Using ORM::Factory with no ORM

Plain Raku classes work with the generic adapter:

```perl6
class User {
  has Str  $.fname is rw;
  method save-bang { self }                     # or .save, or omit if you only build
}

define {
  .factory: 'user', :class(User), {
    .fname: 'Greg';
  };
};

ORM::Factory.build('user');     # User.new(fname => 'Greg')
ORM::Factory.create('user');    # then .save-bang
```

For a non-AR, non-trivial persistence layer, write a custom adapter — see
[Persistence](/docs/ORM-Factory/persistence#writing-a-custom-adapter).

## Pairing with a data-generation library

`ORM::Factory` deliberately ships no fake-data generator. Pair it with
whatever you like:

```perl6
use Fake::Person;       # any Faker-equivalent

define {
  .sequence: 'email-id';

  .factory: 'user', {
    .fname: { Fake::Person.first-name };
    .lname: { Fake::Person.last-name };
    .email: { Fake::Person.email(:id(ORM::Factory.generate('email-id'))) };
  };
};
```

Sequences (for guaranteed uniqueness) and the data generator (for realism)
play together cleanly: use the sequence to scope the random value.

## Concurrency and performance

See [Concurrency](/docs/ORM-Factory/concurrency) and [Performance](/docs/ORM-Factory/performance) for the
guarantees and the regression guards.
