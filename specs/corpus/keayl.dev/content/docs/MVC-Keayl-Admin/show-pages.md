---
title: Show pages
slug: show-pages
order: 9
description: The show page renders one record from its declared attributes. Each attribute is a detail row, using the same formatters as the index, so a value renders...
toc: true
---
The show page renders one record from its declared `attribute`s. Each attribute
is a detail row, using the same formatters as the index, so a value renders the
same way in both places.

```raku
attribute('title');
attribute('published', :format<boolean>);
attribute('body',      :display({ .body.substr(0, 200) }));
attribute('author');     # an association
```

A `:display` block computes the row value; otherwise the value is read from the
column and run through the named `:format`.

An attribute value is HTML-escaped by default. Declare an attribute `:html` to
render its value as trusted markup instead, bypassing escaping and any `:format`
(see [raw markup](/docs/MVC-Keayl-Admin/index-pages#raw-markup)):

```raku
attribute('preview', :html, :display({ .rendered-html }));
```

## Action panel

A sidebar panel holds the per-record actions: Edit (links to the edit form),
Delete (a destroying form with a confirmation), and any custom member actions
declared for the resource. Each control is hidden when the authorization policy
forbids it. See [Destroy, batch, and custom actions](/docs/MVC-Keayl-Admin/actions).

## Associations

When an attribute names an association, the show page renders it from the
relationship rather than as a column:

- A **belongs-to** (or has-one) attribute renders as a link to the associated
  record's admin show page, when that model is itself a registered resource. A
  missing association renders a dash, never a broken link.
- A **has-many** (or has-and-belongs-to-many) attribute renders as a compact
  sub-table, each row linking into the associated resource, capped with an "and
  N more" line. A `:display` block overrides this, summarising the collection
  however you like (for example a comma-joined string) in place of the per-record
  links. The summary is HTML-escaped unless the attribute is declared `:html`.

Association kind and target are detected through the ORM's reflection, so the
link targets are resolved from the registry by model.
