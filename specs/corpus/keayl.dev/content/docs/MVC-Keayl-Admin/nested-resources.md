---
title: Nested resources
slug: nested-resources
order: 10
description: A resource that belongs to another can be nested under its parent, scoping every view to the parent record, in the spirit of Active Admin's belongs_to.
toc: true
---
A resource that belongs to another can be nested under its parent, scoping every
view to the parent record, in the spirit of Active Admin's `belongs_to`.

```raku
MVC::Keayl::Admin.register(Post, {
  belongs-to('author');

  column('title');
  field('title', :as<string>);
  permit(<title>);
});
```

`belongs-to` names a `belongs-to` association on the model. The admin reads the
target class and foreign key from the ORM reflection.

## Nested routes and scoping

Alongside the standalone routes, the resource gains a nested set under its
parent: `/admin/authors/:author-id/posts`, `/admin/authors/:author-id/posts/:id`,
and so on. Within those:

- the index, show, edit, update, and destroy relations are scoped to the parent,
  so a record under a different parent is a 404;
- the breadcrumbs show the parent chain (Authors, the author, Posts);
- the new button, row actions, and form post into the nested path;
- a create fixes the foreign key to the parent, so a new record belongs to it.

The standalone routes still work and list every record. The nested resource is
left out of the top menu; reach it from the parent's show page, which links to
each resource nested under it.

## Eager loading

`includes` names associations to eager-load on the index, avoiding a query per
row when a column or block reads an association:

```raku
MVC::Keayl::Admin.register(Post, {
  includes('author');

  column('author', :display({ .author.name }));
});
```
