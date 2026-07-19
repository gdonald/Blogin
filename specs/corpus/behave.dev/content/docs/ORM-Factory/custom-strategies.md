---
title: Custom strategies
slug: custom-strategies
order: 19
description: ORM::Factory ships with four built-in strategies (build, create, attributes-for, build-stubbed). You can register your own at runtime — or replace a...
toc: true
---
`ORM::Factory` ships with four built-in strategies (`build`, `create`,
`attributes-for`, `build-stubbed`). You can register your own at runtime —
or replace a built-in — to integrate serialisation, alternative persistence
flows, or any other build pipeline.

## Registering a strategy

A strategy is any class that does the `ORM::Factory::Strategy` role:

```raku
use ORM::Factory;

class JsonStrategy does ORM::Factory::Strategy {
  method to-sym(--> Str) { 'json' }

  method result(ORM::Factory::Evaluator $eval) {
    my %h = $eval.attributes-hash(:skip-associations);
    to-json(%h);
  }

  method association(Str:D $name, @variants, %overrides) {
    ORM::Factory.build($name, |@variants, |%overrides);
  }
}

ORM::Factory.register-strategy('json', JsonStrategy);
```

Once registered, the strategy is dispatched by name via `FALLBACK`, so the
top-level helper is simply the name itself:

```raku
ORM::Factory.json('user');                     # → JSON for the user factory
ORM::Factory.json('user', 'admin', :name<Pat>); # variants + overrides flow through
```

## Inspecting the registry

```raku
ORM::Factory.strategies.keys;       # registered names
ORM::Factory.strategy-class-for('json');  # the class
ORM::Factory.strategy-for('json');        # a fresh, persistence-wired instance
```

## Re-registering a built-in

`register-strategy` accepts any built-in name, so a project can swap the
default `build` behaviour without monkey-patching:

```raku
class ShoutBuildStrategy does ORM::Factory::Strategy {
  method to-sym(--> Str) { 'build' }
  method result(ORM::Factory::Evaluator $eval) {
    my $instance = $!persistence.instantiate($eval.factory.lookup-class, $eval.attributes-hash);
    $instance.name = $instance.name.uc if $instance.^can('name');
    $instance;
  }
  method association(Str:D $name, @variants, %overrides) {
    ORM::Factory.build($name, |@variants, |%overrides);
  }
}

ORM::Factory.register-strategy('build', ShoutBuildStrategy);
ORM::Factory.build('user').name;  # → uppercased

# Restore the default
ORM::Factory.register-strategy('build', ORM::Factory::BuildStrategy);
```

## Per-strategy association cascade

`association` is the cascade hook. A strategy controls how every implicit or
explicit association of the in-flight factory is built. For example, a
strategy that always stubs associations regardless of the top-level call:

```raku
class StubAssocStrategy does ORM::Factory::Strategy {
  method to-sym(--> Str) { 'cascade-stub' }
  method result(ORM::Factory::Evaluator $eval) {
    $!persistence.instantiate($eval.factory.lookup-class, $eval.attributes-hash);
  }
  method association(Str:D $name, @variants, %overrides) {
    ORM::Factory.build-stubbed($name, |@variants, |%overrides);
  }
}

ORM::Factory.register-strategy('cascade-stub', StubAssocStrategy);
ORM::Factory.cascade-stub('article');  # article's author is stubbed
```
