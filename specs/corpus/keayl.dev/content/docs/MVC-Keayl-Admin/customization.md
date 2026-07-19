---
title: Customization
slug: customization
order: 18
description: Each index exports its current scoped-and-filtered view. The toolbar offers the enabled formats, and the export honours the active scope, filters, and...
toc: true
---
## Export

Each index exports its current scoped-and-filtered view. The toolbar offers the
enabled formats, and the export honours the active scope, filters, and sort.

- `GET /admin/<resource>/export.csv` streams a CSV: a header row of column labels
  followed by one row per record, with values quoted when they contain a comma,
  quote, or newline. It is sent as a download attachment and streamed rather than
  buffered, so large result sets do not build a single string in memory.
- `GET /admin/<resource>/export.json` returns the same view as JSON, rendered
  through the controller's custom-renderer registry. Each object carries the
  column values.
- `GET /admin/<resource>/export.xml` returns the same view as XML through the
  renderer registry, one `<record>` per row.

### Export columns

By default the export uses the index columns. A `csv` block declares an explicit
set of export columns instead, applied to CSV, JSON, and XML alike:

```raku
csv({
  column('title');
  column('author', :display({ .author.name }));
});
```

### Limiting the formats

The export is on by default with all three formats. Restrict or disable it per
resource with the `export` option:

```raku
MVC::Keayl::Admin.register(Post, { ... }, export => <csv json>);   # only CSV and JSON
MVC::Keayl::Admin.register(Log,  { ... }, export => False);        # no export
```

Only the offered formats get a route and a toolbar link; a disabled format is a
404.

## Standalone pages

A page has no backing model. It is registered with a block that returns HTML,
rendered inside the admin layout, and added to the menu:

```raku
MVC::Keayl::Admin.page('reports', -> $controller {
  '<div class="card"><div class="card-body">Sales summary</div></div>'
}, title => 'Reports', group => 'Tools', icon => 'graph-up');
```

The block receives the controller, so it can read params or the current admin.
Pass `:hide` to register the route without a menu entry. Page menu entries honour
the same `group`, `priority`, and `icon` options as resources. A page's `icon`
also appears in its heading, the same as a resource's [icon](/docs/MVC-Keayl-Admin/navigation).

## Localization

All chrome, resource names, attribute labels, scope names, and action labels go
through an I18n backend. Without translations they fall back to the humanized
English defaults, so localization is opt-in. Humanizing title-cases each word and
drops a trailing `_id` or `_at` suffix, so `author_id` becomes `Author` and
`created_at` becomes `Created`.

Load locale files and select a locale:

```raku
MVC::Keayl::Admin.load-locales('config/locales');
MVC::Keayl::Admin.locale('fr');
```

The lookup keys are:

- `activerecord.models.<model>` and `activerecord.attributes.<model>.<attr>` for
  resource and attribute names (shared with the ORM's conventions).
- `keayl_admin.chrome.<key>` for chrome strings such as `dashboard`, `actions`,
  `show`, `edit`, `delete`, `new`, `filters`, and `no-records`.
- `keayl_admin.scopes.<name>` for scope tab labels.
- `keayl_admin.actions.<name>` for custom action labels.

## Theming

### Host view overrides

Register a view path that is searched before the engine's own views. A host
template at the same name overrides the engine's:

```raku
MVC::Keayl::Admin.view-path('app/views/admin');
```

A file at `app/views/admin/resource/index.html.haml` then replaces the built-in
index template, and likewise for any partial.

### CSS variables

The admin layers a small stylesheet over the Bootstrap bundle that exposes a set
of CSS variables:

- `--keayl-admin-sidebar-bg`
- `--keayl-admin-sidebar-width`
- `--keayl-admin-navbar-bg`
- `--keayl-admin-accent`
- `--keayl-admin-body-bg`

Override them with a host stylesheet, which is loaded after the theme so its
values win:

```raku
MVC::Keayl::Admin.use-stylesheet('/assets/admin-theme.css');
```

```css
:root {
  --keayl-admin-navbar-bg: #1b1e21;
  --keayl-admin-accent: #6f42c1;
}
```
