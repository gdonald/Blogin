---
title: Command-line interface
slug: cli
order: 3
description: bin/keayl drives an application from the shell. It is a thin dispatcher over MVC::Keayl::CLI, where each command is a plain function, so the same behavior...
toc: true
---
`bin/keayl` drives an application from the shell. It is a thin dispatcher over
`MVC::Keayl::CLI`, where each command is a plain function, so the same behavior is
available to call directly and to test.

```
keayl <command> [options]
```

Run `keayl help` for the command list and `keayl version` for the installed
version.

## new

Scaffold a new application skeleton in a subdirectory named after the app:

```
keayl new blog
```

This writes a starter layout: `config/application.json`, `config/application.raku`
(which loads the config and routes, wires static asset serving, and returns an
`MVC::Keayl::Application`), `config/routes.raku` with a `root` route, a
`HomeController`, an `application` layout, a HAML view for the `index` action,
`bin/server`, `bin/dev`, and `bin/test` scripts, an `assets/` directory with a
favicon and stylesheet, a browser spec for the home page, a `META6.json`, a
`README.md`, and a `.gitignore`.

`scaffold-app($name, :$into)` returns the list of files it created, relative to
the new application directory. `:into` defaults to the current directory. The
`bin/` scripts are written executable.

## Running a new app

A scaffolded app ships its own runner scripts so it does not depend on `keayl`
being on the PATH:

```
bin/dev
```

`bin/dev` sets the development environment and host/port, then execs `bin/server`,
which loads `config/application.raku` and serves it with the Cro adapter. Both
honor `KEAYL_HOST` and `KEAYL_PORT`; `bin/dev` defaults them to `127.0.0.1:3000`.

```
bin/test
```

`bin/test` runs the `specs/` with behave, filtering the benign socket-teardown
noise that browser specs produce. Setting `SHOW_CHROME` runs the browser specs in
a visible window, serially.

The generated `META6.json` depends on `MVC::Keayl` and `ORM::ActiveRecord`, and
test-depends on `BDD::Behave`, `BDD::Behave::Playwright`, `WWW::Playwright`, and
`ORM::Factory`, so `zef install --deps-only .` provisions everything the app and
its specs need.

## server

Boot the application and serve it over HTTP with the Cro adapter:

```
keayl server
keayl s --host 0.0.0.0 --port 8080
```

`server` loads `config/application.raku`, builds the endpoint, and listens until
interrupted. The host defaults to `127.0.0.1` and the port to `3000`.

`build-server($app, :$host, :$port, :$scheme)` constructs the
[`Adapter::Cro`](/docs/MVC-Keayl/adapters) around the application's endpoint without binding a
socket, so the wiring can be inspected on its own.

## routes

Print the route table for `config/routes.raku`:

```
keayl routes
```

Each row lists the route name, its verbs (joined with a comma), the path, and the
target. `routes-table(@table)` renders the table that
[route introspection](/docs/MVC-Keayl/routing) produces; an unnamed route renders with a blank
name column.

## console

Open a REPL with the application booted and available:

```
keayl console
keayl c
```

`console` loads and boots `config/application.raku`, binds it to the
`$*KEAYL-APP` dynamic variable, then reads lines and evaluates each in turn,
printing the result. An evaluation error is reported without ending the session.

```
keayl> $*KEAYL-APP.environment
development
```

## generate

Run a generator to write controllers, views, models, and route stubs:

```
keayl generate controller posts index show
keayl g scaffold post
```

Each generated file is reported as `create`; an existing file is reported as
`exists` and left untouched, so a generator can be re-run safely. Names are
inflected with a small built-in inflector (`camelize`, `pluralize`,
`singularize`) covering the common cases; irregular words may need editing.

### controller

```
keayl generate controller posts index show
```

Writes `app/controllers/PostsController.rakumod` with a method per action, a HAML
view under `app/views/posts/` for each action, and a `get` route stub per action
inserted into `config/routes.raku`. An action method left empty renders its view
through the controller's implicit render.

### scaffold

```
keayl generate scaffold post
```

Takes a singular resource name and writes the full CRUD set: an
[`ORM::ActiveRecord`](/docs/MVC-Keayl/application) model (`app/models/Post.rakumod`), a
controller with `index`, `show`, `new`, `create`, `edit`, `update`, and `destroy`
actions, HAML views (`index`, `show`, `new`, `edit`, and a shared `_form`
partial), and a `resources` route inserted into `config/routes.raku`. The
resource name is pluralized for the controller, views, and route.

### mailer, job, channel

```
keayl generate mailer user welcome
keayl generate job cleanup
keayl generate channel chat
```

`mailer` writes a `MVC::Keayl::Mailer` subclass with a method per action and an
html and text view for each. `job` writes a `MVC::Keayl::Job` subclass with a
`perform` method. `channel` writes a `MVC::Keayl::Cable::Channel` subclass with
`subscribed` and `unsubscribed` callbacks. Each also writes a matching `t/` test
and `specs/` spec stub.

### model, migration, resource

```
keayl generate model post title:string body:text
keayl generate migration create-comments body:text
keayl generate resource post title:string
```

`model` writes an [`ORM::ActiveRecord`](/docs/MVC-Keayl/application) model and a timestamped
`db/migrate` migration that creates the table, with each `name:type` becoming a
typed column. `migration` writes just the migration. `resource` writes a model
plus a controller and a `resources` route. Fields default to `string` when no
type is given.

### helper

```
keayl generate helper posts
```

Writes a helper module (`app/helpers/PostsHelper.rakumod`). The `controller` and
`scaffold` generators also write a matching helper module, and `keayl new` writes
a global `ApplicationHelper`. See [view helpers](/docs/MVC-Keayl/helpers) for how helper module
subs become bare template calls.

### Route insertion

A generator inserts its route stubs just inside the `routes` (or `draw`) block of
`config/routes.raku`, skipping any stub already present. If the file has no
routes block, the generator still writes the other files and reports that it
could not add the routes.

## Defaults a new app ships

`keayl new` writes a few framework defaults:

- A health-check endpoint at `/up`
  (`MVC::Keayl::HealthController`) that returns `200` with a green page.
- A PWA manifest at `/manifest.json` and a service worker at `/service-worker.js`
  (`MVC::Keayl::PWAController`).
- Static exception pages `public/404.html`, `public/422.html`, and
  `public/500.html`.
- An `application` layout under `app/views/layouts/` that the `HomeController`
  renders through, linking the generated favicon and stylesheet.
- `MVC::Keayl::Middleware::Static` prepended to the stack, serving the `assets/`
  directory under `/assets`.

The health and PWA controllers are registered in the generated
`config/application.raku`, and the routes are wired in `config/routes.raku`.
