---
title: Sequences
slug: sequences
order: 11
description: A *sequence* is a named counter that yields a new value on every call. Use sequences when you want unique attribute values across factory builds — emails,...
toc: true
---
A *sequence* is a named counter that yields a new value on every call. Use
sequences when you want unique attribute values across factory builds —
emails, login names, slugs.

## Global sequences

Declare with `.sequence` inside a `define` block; consume with
`ORM::Factory.generate`:

```perl6
define {
  .sequence: 'email', -> $n { "user{$n}\@example.com" };
};

ORM::Factory.generate('email');                 # 'user1@example.com'
ORM::Factory.generate('email');                 # 'user2@example.com'
ORM::Factory.generate-list('email', 3);         # the next three values
```

Without a block, a sequence returns its raw counter:

```perl6
define { .sequence: 'count'; };
ORM::Factory.generate('count');                 # 1
ORM::Factory.generate('count');                 # 2
```

## Custom start value

`:start` controls the first value. Numeric starts increment with `+ 1`;
string starts advance with `.succ`:

```perl6
define {
  .sequence: 'id',     :start(1000);
  .sequence: 'letter', :start('a');
};

ORM::Factory.generate('id');                    # 1000
ORM::Factory.generate('id');                    # 1001

ORM::Factory.generate('letter');                # 'a'
ORM::Factory.generate('letter');                # 'b'
```

## Custom iterator

If a `+ 1` / `.succ` progression isn't enough, pass any `Iterator` and the
sequence pulls from it (with an optional block to transform each value):

```perl6
define {
  .sequence: 'fib', :iterator((1, 1, *+* ... *).iterator);
};

ORM::Factory.generate-list('fib', 5);           # (1, 1, 2, 3, 5)
```

The sequence raises `X::ORM::Factory::UsageError` if the iterator is
exhausted.

## Inline sequences (per-factory)

Declared inside a factory body, a sequence is bound as an attribute on that
factory and advances on every build:

```perl6
define {
  .factory: 'invitation', :class(Invitation), {
    .sequence: 'token', -> $n { "tok-$n" };
  };
};

ORM::Factory.attributes-for('invitation')<token>;   # 'tok-1'
ORM::Factory.attributes-for('invitation')<token>;   # 'tok-2'
```

Inline sequences are isolated per factory; they have no name in the global
sequence registry.

## Resetting counters

`ORM::Factory.rewind-sequences` resets every global sequence's counter back
to its `:start`. It is the standard between-tests cleanup for any spec that
asserts on sequence values:

```perl6
before-each {
  ORM::Factory.rewind-sequences;
}
```

`reload` also clears the sequence registry along with factory definitions
and aliases.

## Errors

| Exception                              | Trigger                                          |
|----------------------------------------|--------------------------------------------------|
| `X::ORM::Factory::UnknownSequence`     | `generate` on an unregistered name.              |
| `X::ORM::Factory::DuplicateSequence`   | Two `.sequence 'x', …` calls with the same name. |
| `X::ORM::Factory::UsageError`          | Custom iterator exhausted.                       |
