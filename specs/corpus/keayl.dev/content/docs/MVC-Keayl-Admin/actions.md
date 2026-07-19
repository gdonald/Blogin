---
title: Actions
slug: actions
order: 12
description: Each index row has a delete control that issues an HTMX DELETE and removes the row in place on success. Before the request goes out, a Bootstrap...
toc: true
---
## Destroy

Each index row has a delete control that issues an HTMX `DELETE` and removes the
row in place on success. Before the request goes out, a Bootstrap confirmation
modal asks the operator to confirm. The show page deletes through a plain form
that redirects back to the index, guarded by the same modal, and still submits
directly when JavaScript is unavailable. Dependent handling (cascading,
nullifying) is delegated to the ORM's `destroy`.

## Batch actions

The index wraps its rows in a form with a per-row selection checkbox, a
select-all-on-page control, and a selected-count indicator. A toolbar applies a
chosen action to the selected ids. Batch **Destroy** is always available, and a
`batch-action` declares a custom bulk operation whose block receives the selected
records:

```raku
batch-action('Publish', -> @records {
  .update({ published => True }) for @records;
});
```

## Custom member and collection actions

`member-action` and `collection-action` declare developer-defined actions. Each
generates a route and a button (on the show page for member actions, on the index
for collection actions), with an optional confirmation shown in the same Bootstrap
modal. The handler block
receives the controller and either the record (member) or the relation
(collection), and returns a response — a redirect or a rendered partial.

```raku
member-action('approve', -> $controller, $record {
  $record.update({ approved => True });
  $controller.redirect-to($controller.path-for('post', $record.id));
}, :confirm('Approve this post?'));

collection-action('publish-all', -> $controller, $relation {
  .update({ published => True }) for $relation.all;
  $controller.redirect-to($controller.path-for('posts'));
});
```
