---
title: Mounting
slug: mounting
order: 3
description: The admin ships as a MVC::Keayl engine. A host application mounts it at a path, the same way it mounts any other engine.
toc: true
---
The admin ships as a `MVC::Keayl` engine. A host application mounts it at a
path, the same way it mounts any other engine.

```raku
use MVC::Keayl::Admin;

routes {
  mount MVC::Keayl::Admin.endpoint, at => MVC::Keayl::Admin.config.mount-path;
}
```

`MVC::Keayl::Admin.endpoint` returns a dispatcher for the admin engine. The
engine isolates the `MVC::Keayl::Admin` namespace and carries its own view,
helper, and asset paths, so its templates and assets never collide with the
host application's.

## Configuration

`MVC::Keayl::Admin.config` returns the shared configuration. Two settings are
available:

| Setting       | Default    | Purpose                                  |
| ------------- | ---------- | ---------------------------------------- |
| `mount-path`  | `/admin`   | The path the host mounts the engine at.  |
| `site-title`  | `Admin`    | The title shown in the admin chrome.     |
| `logout-path` | (none)     | The path the navbar logout link targets. |

Set them with `configure`:

```raku
MVC::Keayl::Admin.configure(
  mount-path  => '/manage',
  site-title  => 'Control Panel',
  logout-path => '/manage/logout',
);
```

`mount-path` is read by the host when it mounts the engine. `site-title` is
read on every request and exposed to controllers and views through the
per-request admin context.

## The admin context

Every admin controller derives from `MVC::Keayl::Admin::Controller`, which runs
a `before-action` that builds a per-request admin context. The context holds the
site title, the mount path, and the current request path.

```raku
self.admin-context<site-title>;
self.admin-context<path>;
```
