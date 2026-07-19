---
title: Index pages
slug: index-pages
order: 5
description: The index page lists a resource's records in a table built from the declared columns. A generic controller resolves the resource from the URL, queries the...
toc: true
---
The index page lists a resource's records in a table built from the declared
`column`s. A generic controller resolves the resource from the URL, queries the
model's ORM relation, and renders the table.

## Columns

Each `column` becomes a table column. The header is the humanized column name.
A column renders its value one of three ways:

- a plain attribute read from the record (`column('title')`);
- a `display` block, called with the record, for a computed value
  (`column('author', :display({ .author.name }))`);
- a `format`, applied to the attribute value.

## Formatters

`MVC::Keayl::Admin::Formatter` provides value formatters shared by the index and
the show page: `date`, `time`, `datetime`, `currency`, `boolean`, and
`truncate`. A column with `:format<boolean>` renders Yes/No, `:format<date>`
renders the date portion, and so on.

`:format<link-to-show>` is special: it links the value to the record's show
page, so a title column can double as the row's primary link.

`:format<status-tag>` renders the value as a colored Bootstrap badge, reused on
the show page. A boolean renders as a green Yes or a grey No; a string maps known
states to colors (`active`/`published`/`approved` green, `pending` amber,
`draft`/`inactive` grey, `rejected`/`failed` red) and humanizes the label,
defaulting to grey for an unknown value. Pair it with a `display` block to derive
the state from the record:

```raku
column('state', :format<status-tag>, :display({ .published ?? 'active' !! 'draft' }));
```

## Raw markup

Column values are HTML-escaped by default, so a `display` block that returns a
string with angle brackets renders as literal text. Declare a column `:html` when
the value is trusted markup that should render as-is:

```raku
column('preview', :html, :display({ '<a href="' ~ .url ~ '">open</a>' }));
```

An `:html` column emits its value verbatim, bypassing escaping and any `:format`.
You own the safety of the markup, so escape untrusted parts yourself. The same
flag works on the table, grid, and blog presentations, and on show-page
attributes.

## Rows and actions

Every row ends in an actions column with Show, Edit, and Delete controls. Delete
issues an HTMX destroy after a Bootstrap confirmation modal and removes the row in
place; see
[Destroy, batch, and custom actions](/docs/MVC-Keayl-Admin/actions). When a resource has no records,
the table shows an empty state instead of rows, and the page carries a New-record
button linking to the new form. Row controls are hidden when the authorization
policy forbids the action.

A toolbar row sits above the table: batch controls on the left, and the
collection actions, New-record button, and Filters button on the right. Below the
table, a footer row holds the pagination and record summary on the left and the
export links on the right.

## Sorting

A column declared `:sortable` renders its header as a link. Clicking it sorts the
index by that column, adding an `ORDER BY` to the relation. The sort column and
direction travel in the query string (`?sort=title&dir=desc`); the active column
shows a direction arrow, and clicking it again toggles ascending and descending.
Only declared sortable columns are accepted, so the order clause is never built
from arbitrary input.

When no column is requested in the query string and the resource declares no
`sort-order` default, the index falls back to ordering by `id` descending, so the
newest records appear first.

## Pagination

The index applies `LIMIT` and `OFFSET` for the current page. The page size
defaults to 25 and is configurable per resource:

```raku
MVC::Keayl::Admin.register(Post, { ... }, per-page => 50);
```

A pagination control renders in the footer below the table, next to a page
summary (`Showing 1–25 of 218`). The links carry the current sort state so it
survives navigation. When every record fits on one page the control collapses to
just the summary.

## HTMX

The table and pagination live inside an `#admin-index` container. Sort-header and
pagination links issue an `hx-get` that targets that container, so the table body
swaps in place without a full-page reload. Each link also has a plain `href`, so
with JavaScript disabled the same action degrades to a full-page request.
