---
title: Installation
slug: installation
order: 2
description: Add MVC::Keayl::Admin to your application's dependencies and install it. It brings the vendored Bootstrap and HTMX assets with it.
toc: true
---
Add `MVC::Keayl::Admin` to your application's dependencies and install it. It
brings the vendored Bootstrap and HTMX assets with it.

## Install the admin

The generators ship as a `keayl-admin` command. From your application root:

```
keayl-admin generate admin:install
```

This:

- mounts the engine in `config/routes.raku` (`mount MVC::Keayl::Admin.endpoint, at => '/admin'`),
- writes `config/initializers/admin.raku` with a `configure` call for the site
  title and mount path,
- writes an `app/admin/dashboard.raku` dashboard block stub,
- writes a `config/initializers/admin_authentication.raku` authentication stub,
- generates parallel `t/` and `specs/` coverage that the admin engine mounts.

The mount is idempotent: running install again does not duplicate it.

## Generate an admin for a model

```
keayl-admin generate admin Post
```

The generator loads the named model from the conventional `app/models` directory
(with `app/models/concerns` on the search path for its dependencies), introspects
its schema at generate time, and emits an explicit registration at
`app/admin/post.raku` with a `column`, `field`, and `filter` for each column, and
a `permit` of the editable attributes (the primary key and timestamps are left
out of the editable set). Field and filter types follow the column types. If the
model cannot be loaded, the generator reports the error and exits non-zero without
writing anything. Because the file is explicit, edit it freely afterward. Parallel
`t/` and `specs/` coverage is generated alongside it.

## Mounting by hand

If you prefer to wire things up yourself, mounting is a single line in your
routes. See [Mounting](/docs/MVC-Keayl-Admin/mounting) and [Registering resources](/docs/MVC-Keayl-Admin/registration).
