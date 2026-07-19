---
title: Getting Started
slug: getting-started
order: 2
description: A Behave project keeps spec files in a specs/ directory at the project root. The behave runner picks up any file matching spec.raku (e.g....
toc: true
---
## Project layout

A Behave project keeps spec files in a `specs/` directory at the project root. The `behave` runner picks up any file matching `spec.raku` (e.g. `001-basic-spec.raku`, `users-spec.raku`, `subdir/admin-spec.raku`).

```
my-project/
├── lib/
│   └── MyApp.rakumod
└── specs/
    ├── basic-spec.raku
    └── users/
        └── auth-spec.raku
```

## Your first spec

Every spec file starts with `use BDD::Behave;` and then declares one or more top-level `describe` blocks.

```raku
use BDD::Behave;

describe 'arithmetic', {
  it 'adds integers', {
    expect(1 + 1).to.be(2);
  }

  it 'multiplies integers', {
    expect(3 * 4).to.be(12);
  }
}
```

## Running specs

Run all specs found in `specs/`:

```shell
behave
```

Run a single spec file:

```shell
behave specs/basic-spec.raku
```

During local development of an app whose `lib/` is not yet installed, prefix with `raku -Ilib`:

```shell
raku -Ilib bin/behave specs/basic-spec.raku
```

See [Running Specs](/docs/BDD-Behave/running) for the full set of options.

## Where to go next

- [`describe` / `context`](/docs/BDD-Behave/describe): group related examples
- [`it`](/docs/BDD-Behave/it): define an example
- [`let`](/docs/BDD-Behave/let): lazy, memoized values per example
- [Hooks](/docs/BDD-Behave/hooks): `before-each`, `after-each`, `before-all`, `after-all`
- [Shared Contexts](/docs/BDD-Behave/shared-contexts): reusable `let`s and hooks via `shared-context` / `include-context`
- [`expect`](/docs/BDD-Behave/expect): assertions
