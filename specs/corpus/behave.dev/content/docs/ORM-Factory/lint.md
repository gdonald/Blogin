---
title: Linting & diagnostics
slug: lint
order: 24
description: ORM::Factory.lint builds every registered factory through a chosen strategy and aggregates the failures. It is the simplest way to confirm a suite of...
toc: true
---
## `lint`

`ORM::Factory.lint` builds every registered factory through a chosen
strategy and aggregates the failures. It is the simplest way to confirm a
suite of factories still produces valid records:

```raku
ORM::Factory.lint;                    # every factory, create-strategy
ORM::Factory.lint(:strategy<build>);  # build-strategy only
ORM::Factory.lint(:variants);         # each factory × variant combination
ORM::Factory.lint(:verbose);          # one progress line per attempt
ORM::Factory.lint('user', 'post');    # only the named factories
```

Healthy factories return silently. Failures are collected — every factory
is attempted, even if earlier ones failed — and the aggregated report is
attached to a single `X::ORM::Factory::LintFailures` exception:

```raku
try {
  ORM::Factory.lint(:variants);
  CATCH {
    when X::ORM::Factory::LintFailures {
      say .message;
      for .failures -> $f {
        say "factory: $f<factory>, variant: $f<variant>, error: $f<error>";
      }
    }
  }
}
```

When the active persistence adapter implements `is-valid` and returns
`False`, `lint` raises `X::ORM::Factory::InvalidRecord` for that factory and
records it in the same report.

## Introspection

The registry is queryable without touching the underlying hash:

| method                                    | returns                                              |
| ----------------------------------------- | ---------------------------------------------------- |
| `factory-names`                           | every registered factory name (sorted)               |
| `sequence-names`                          | every registered sequence name (sorted)              |
| `global-variant-names`                    | every top-level variant name (sorted)                |
| `variant-names-for($name)`                | variants visible to one factory (own + inherited)    |
| `dump-attributes($name, *@variants, *%overrides)` | each attribute's transient/association/dynamic flags |
| `describe-factory($name)`                 | `{name, class-name, parent, ancestors, aliases, variants, attributes}` |

```raku
ORM::Factory.factory-names;                                # ('post', 'user')
ORM::Factory.describe-factory('user')<variants>;           # ('admin',)
ORM::Factory.dump-attributes('post')<author><association>; # True
```

## Error taxonomy

Every error raised by the factory engine descends from `X::ORM::Factory`:

| class                                  | raised when                                                      |
| -------------------------------------- | ---------------------------------------------------------------- |
| `X::ORM::Factory::UnknownFactory`      | a factory or alias is not registered                             |
| `X::ORM::Factory::DuplicateFactory`    | `define` re-registers an existing name                           |
| `X::ORM::Factory::UnknownVariant`      | a variant name is not visible from the in-flight factory         |
| `X::ORM::Factory::DuplicateVariant`    | `variant` re-registers an existing name                          |
| `X::ORM::Factory::DuplicateAlias`      | an alias collides with another alias or a factory name           |
| `X::ORM::Factory::UnknownClass`        | name → class resolution fails and no `:class` is supplied        |
| `X::ORM::Factory::UnknownAttribute`    | the evaluator is asked for an attribute that is not declared     |
| `X::ORM::Factory::UnknownSequence`     | `generate` is called with an unknown sequence name               |
| `X::ORM::Factory::DuplicateSequence`   | `sequence` re-registers an existing name                         |
| `X::ORM::Factory::MissingAssociation`  | an association points at a factory that is not registered        |
| `X::ORM::Factory::CyclicAssociation`   | associations form a cycle while building                         |
| `X::ORM::Factory::UnknownCallback`     | a custom callback name is referenced before it is registered     |
| `X::ORM::Factory::UnknownStrategy`     | `strategy-for` is given an unregistered name                     |
| `X::ORM::Factory::UsageError`          | the DSL is called with the wrong shape (e.g. wrong arity)        |
| `X::ORM::Factory::InvalidRecord`       | an adapter reports validation failure (carries `record`, `errors`, `factory-name`) |
| `X::ORM::Factory::LintFailures`        | `lint` finished with one or more failures (carries `failures`)   |

Every exception carries a human-readable `.message`. Structured exceptions
(`InvalidRecord`, `LintFailures`) carry extra fields you can pattern-match
on instead of parsing the text.
