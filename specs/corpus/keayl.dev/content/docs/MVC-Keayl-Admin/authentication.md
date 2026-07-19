---
title: Authentication
slug: authentication
order: 14
description: Every admin request runs through a pluggable authentication gate. Configure a strategy and the gate challenges or redirects unauthenticated visitors...
toc: true
---
Every admin request runs through a pluggable authentication gate. Configure a
strategy and the gate challenges or redirects unauthenticated visitors before
any action runs. With no strategy configured the admin is open, which is
convenient in development but should not be shipped to production.

```raku
use MVC::Keayl::Admin;
use MVC::Keayl::Admin::Authentication::Basic;

MVC::Keayl::Admin.authenticate-with(
  MVC::Keayl::Admin::Authentication::Basic.new(
    name     => 'admin',
    password => 'secret',
    realm    => 'Admin',
  )
);
```

## Strategies

A strategy does the `MVC::Keayl::Admin::Authentication::Strategy` role, which has
two methods: `authenticate($controller)` returns the current admin or an
undefined value, and `challenge($controller)` issues the response for an
unauthenticated visitor.

### HTTP basic

`MVC::Keayl::Admin::Authentication::Basic` compares the request's basic
credentials against a configured `name` and `password` using a constant-time
comparison, and challenges with a `401` and a `WWW-Authenticate` header.

### Session

`MVC::Keayl::Admin::Authentication::Session` reads an id from the session (`key`,
default `admin_id`) and redirects an unauthenticated visitor to `login-path`
(default `/login`). An optional `resolve` block maps the stored id to an admin
object:

```raku
use MVC::Keayl::Admin::Authentication::Session;

MVC::Keayl::Admin.authenticate-with(
  MVC::Keayl::Admin::Authentication::Session.new(
    key        => 'admin_id',
    login-path => '/admin/login',
    resolve    => -> $id { Admin.find($id) },
  )
);
```

## The current admin

When authentication succeeds the result is the current admin. It is available to
controllers as `self.current-admin`, exposed to views as `$current_admin` (the
layout shows a "Signed in as" indicator when present), and is the value the
authorization layer will check.

## Logging out

The gate authenticates but does not own a logout route, since ending a session
depends on the strategy. Configure a `logout-path` and the navbar renders a
logout link next to the "Signed in as" indicator for a signed-in admin. Point it
at a route your app handles, which clears the session and redirects:

```raku
MVC::Keayl::Admin.configure(logout-path => '/admin/logout');
```

Without a configured `logout-path` no logout link is shown.

Assets are served by a controller outside the gate, so the stylesheet, script,
and font bundle load without authentication.
