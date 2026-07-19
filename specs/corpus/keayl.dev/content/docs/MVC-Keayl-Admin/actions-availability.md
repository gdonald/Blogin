---
title: Action availability and toolbar
slug: actions-availability
order: 13
description: By default a resource exposes index, show, new/create, edit/update, and destroy. The actions declaration narrows that set. A disabled action loses its...
toc: true
---
## Enabling and disabling actions

By default a resource exposes index, show, new/create, edit/update, and destroy.
The `actions` declaration narrows that set. A disabled action loses its route
(requests 404), its row controls, and its form links.

```raku
# A read-only resource: only index and show
actions('index', 'show');

# Everything except destroy
actions(except => 'destroy');
```

The action names are `index`, `show`, `new` (covers create), `edit` (covers
update), and `destroy`. Disabling an action also hides its buttons, since the
buttons are gated by the same availability check the routes use.

## Default sort order

`sort-order` sets the ordering applied when the request carries no `sort`
parameter. A user clicking a sortable column header still overrides it.

```raku
sort-order('created-at', :dir<desc>);
```

`dir` is `asc` (default) or `desc`. The column need not be a displayed sortable
column; this is the resource's baseline ordering.

## Disabling filters and batch actions

Filters and batch actions are on by default. Turn either off per resource with a
`register` option:

```raku
MVC::Keayl::Admin.register(Post, { ... }, filters => False, batch-actions => False);
```

With `filters => False` the filter panel, chips, and filter query handling are
gone. With `batch-actions => False` the selection checkboxes, select-all, and the
batch toolbar are gone.

## Toolbar action items

`action-item` adds a button to the page toolbar (the top of the index, above the
show page). The block returns the button's HTML and receives the controller and,
on a show page, the record. It can carry a plain href or an HTMX action.

```raku
action-item('Import', -> $controller, $record {
  '<a class="btn btn-outline-secondary" href="' ~ $controller.path-for('posts') ~ '/import">Import</a>'
}, :only<index>);

action-item('Re-run', -> $controller, $record {
  '<button class="btn btn-outline-secondary" hx-post="/admin/posts/' ~ $record.id ~ '/rerun">Re-run</button>'
}, :only<show>);
```

`only` and `except` restrict the actions an item appears on (`index`, `show`).
Pass `if-can` to gate the item on an authorization ability; it is hidden when the
policy forbids that ability.
