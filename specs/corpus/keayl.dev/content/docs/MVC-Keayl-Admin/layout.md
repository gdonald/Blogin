---
title: Layout and chrome
slug: layout
order: 16
description: Admin pages render inside a Bootstrap 5 layout: a top bar with the site brand, a collapsible left sidebar that spans the full available height, a main...
toc: true
---
Admin pages render inside a Bootstrap 5 layout: a top bar with the site brand, a
collapsible left sidebar that spans the full available height, a main content
region, a flash area, and breadcrumbs. On small screens the sidebar becomes an
offcanvas panel toggled from the top bar. When an admin is signed in, the top bar
shows a "Signed in as" indicator and, if a [logout path](/docs/MVC-Keayl-Admin/authentication) is
configured, a logout link.

## Rendering a page

Controllers derive from `MVC::Keayl::Admin::Controller` and render through
`render-admin`, which wires the chrome (asset tags, brand, menu, breadcrumbs,
flash) into the layout and renders the named template into the main region.

```raku
use MVC::Keayl::Admin::Controller;

unit class MyAdmin::PostsController is MVC::Keayl::Admin::Controller;

method index {
  self.render-admin(
    'posts/index',
    page-title => 'Posts',
  )
}
```

- `page-title` sets the document title and the page heading. It defaults to the
  configured site title. The heading names the current page, so a top-level page
  needs no breadcrumb.
- `breadcrumbs` is a list of `label => url` pairs that render as the ancestor
  trail leading to the current page. Since the heading already names the current
  page, pass only the ancestors and leave the current page out. A nested resource
  supplies its parent chain here; a top-level page omits `breadcrumbs` entirely.

The template named in the first argument renders into the main content region.
The vendored asset bundle, import map, brand, sidebar menu, and breadcrumbs are
supplied by the layout, so a page template only renders its own content.

## The admin context

The base controller runs a `before-action` that builds a per-request context
available as `admin-context` (site title, mount path, and the current path).
