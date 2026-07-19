---
title: Application & configuration
slug: application
order: 2
description: MVC::Keayl::Application ties together the router, middleware stack, and configuration, runs initializers at boot, and builds the endpoint that serves...
toc: true
---
`MVC::Keayl::Application` ties together the router, middleware stack, and
configuration, runs initializers at boot, and builds the endpoint that serves
requests.

```perl6
my $app = MVC::Keayl::Application.new(
  config      => MVC::Keayl::Config.load('config/application.json'),
  controllers => [PostsController, CommentsController],
);

$app.draw-routes({
  root to => 'posts#index';
  resources 'posts';
});

my $endpoint = $app.endpoint;   # boots the app, returns the wrapped endpoint
```

## Configuration

`MVC::Keayl::Config` holds layered settings for an environment. `load` reads a
JSON file (the same `config/application.json` shared with ORM::ActiveRecord),
merging a `shared` section under the selected environment's section. The
environment is chosen from `KEAYL_ENV`, then `RAKU_ENV`, defaulting to
`development`.

```json
{
  "shared":      { "app-name": "Blog" },
  "development":  { "database": { "adapter": "sqlite" }, "log-level": "debug" },
  "production":   { "log-level": "error" }
}
```

Read settings by key or dotted path, and layer overrides with `merge`:

```perl6
$config<app-name>;            # Blog
$config.get('database.adapter');
$config.merge(%( log-level => 'warn' ));
```

## Boot and initializers

`boot` runs registered initializers once, in registration order. Register your
own with `initializer`:

```perl6
$app.initializer('mailer', -> $app { ... });
```

Every application starts with default initializers, including `active-record`,
which calls the application's `database-connector` when the config has a
`database` section, `active-record-connection`, which prepends the per-request
connection middleware when a database is configured (see the middleware page),
`template-haml`, which wires a `Template::HAML`-backed view renderer (reloading
templates outside production), and `assets`, which loads a precompiled
`public/assets/manifest.json` so the view helpers fingerprint asset URLs. See the
asset pipeline page for the manifest details.

## Dispatch

The endpoint recognizes a request against the router and invokes the matching
controller action, with route parameters merged into the controller's params. A
callable route is invoked directly. An unmatched path is `404`, a known path with
the wrong verb is `405`, and an unhandled error is `500` (with the message in
development, terse in production).

Controllers are resolved from the `controllers` list by their controller path
(`PostsController` serves `posts#index`), which is reliable across precompiled
modules.

## Environments

The environment drives behavior: development reloads templates and shows verbose
error pages, production caches templates and shows terse errors. Query it with
`is-development`, `is-test`, `is-production`, and `environment`.
