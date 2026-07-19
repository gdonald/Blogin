---
title: Overview
slug: overview
order: 1
description: The latest version of this documentation lives at https://gdonald.github.io/MVC-Keayl-Admin/.
toc: true
---
The latest version of this documentation lives at [https://gdonald.github.io/MVC-Keayl-Admin/](https://gdonald.github.io/MVC-Keayl-Admin/).

The homepage for MVC::Keayl::Admin is [https://github.com/gdonald/MVC-Keayl-Admin](https://github.com/gdonald/MVC-Keayl-Admin).

## Synopsis

MVC::Keayl::Admin is a generated administration interface for applications built
on [MVC::Keayl](https://github.com/gdonald/MVC-Keayl), in the spirit of Django
admin and Active Admin.

The admin mounts as an engine and depends on the framework's public surface
only. The model layer is delegated to
[ORM::ActiveRecord](https://github.com/gdonald/ORM-ActiveRecord) and view
rendering to [Template::HAML](https://github.com/gdonald/Template-HAML). It
carries a frontend opinion (Bootstrap 5, Bootstrap Icons, HTMX) that MVC::Keayl
deliberately does not. The dependency runs one way: the admin depends on
MVC::Keayl, never the reverse.

## Design principles

- **Explicit, not magic.** Every surface is declared by hand. Nothing is
  inferred at request time.
- **No foreign-key enumeration.** A `select` or association filter requires an
  explicit `collection`, so a large table is never enumerated into a dropdown.
- **Introspection only at generate time.** `keayl generate admin <Model>` reads
  the schema and writes explicit declarations that the developer then edits.
- **Server-rendered, progressive enhancement.** Bootstrap 5 with Bootstrap
  Icons and HTMX, all vendored and served through the asset pipeline. With
  JavaScript disabled every action degrades to a full-page request.
- **Django-style layout.** A left sidebar of grouped, ordered menu entries and
  a right main content area for index, show, and form pages.
