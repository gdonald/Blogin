---
title: Methods syntax
slug: methods-syntax
order: 5
description: use ORM::Factory exports plain-sub wrappers around every public factory method, so specs can stay terse:
toc: true
---
`use ORM::Factory` exports plain-sub wrappers around every public factory
method, so specs can stay terse:

```raku
use ORM::Factory;

define {
  .factory: 'user', { .fname: 'Greg' };
  .sequence: 'counter', -> $n { $n };
};

build('user');              # → ORM::Factory.build('user')
create('user', :role<admin>);
attributes-for('user');
build-stubbed('user');
build-list('user', 5);
create-pair('user');
generate('counter');
```

## Available helpers

| sub                     | delegates to                              |
| ----------------------- | ----------------------------------------- |
| `build`                 | `ORM::Factory.build`                      |
| `create`                | `ORM::Factory.create`                     |
| `build-stubbed`         | `ORM::Factory.build-stubbed`              |
| `attributes-for`        | `ORM::Factory.attributes-for`             |
| `build-list`            | `ORM::Factory.build-list`                 |
| `create-list`           | `ORM::Factory.create-list`                |
| `build-stubbed-list`    | `ORM::Factory.build-stubbed-list`         |
| `attributes-for-list`   | `ORM::Factory.attributes-for-list`        |
| `build-pair`            | `ORM::Factory.build-pair`                 |
| `create-pair`           | `ORM::Factory.create-pair`                |
| `generate`              | `ORM::Factory.generate`                   |
| `generate-list`         | `ORM::Factory.generate-list`              |

## Qualified vs. bare-name syntax

The qualified form (`ORM::Factory.build('user')`) and the bare-name form
(`build('user')`) are both available after `use ORM::Factory`. Pick one style
per spec rather than mixing them.

## behave / Test integration

Both [behave][behave] and the core `Test` module are plain Raku, so the
helpers work in either harness without extra glue:

```raku
use BDD::Behave;
use ORM::Factory;

describe 'user factory', {
  it 'creates a user', { expect(create('user').saved).to.be-truthy };
};
```

```raku
use Test;
use ORM::Factory;

is build('user').fname, 'Greg', 'static attribute';
done-testing;
```

[behave]: https://github.com/gdonald/BDD-Behave
