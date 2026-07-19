---
title: Index presentations and panels
slug: presentations
order: 6
description: By default the index renders a table. A resource can render its records as a card grid or a stacked list instead, by declaring index with an as and a...
toc: true
---
## Index presentations

By default the index renders a table. A resource can render its records as a card
grid or a stacked list instead, by declaring `index` with an `as` and a
per-record block that returns the HTML for one record.

```raku
# A card grid
index(as => 'grid', -> $post {
  '<h5 class="card-title">' ~ $post.read-attribute('title') ~ '</h5>'
  ~ '<p class="card-text">' ~ $post.read-attribute('body') ~ '</p>'
});

# A stacked list
index(as => 'blog', -> $post {
  '<h2>' ~ $post.read-attribute('title') ~ '</h2>'
});
```

`as` is `table` (default), `grid`, or `blog`. The block receives a record and
returns its inner markup; the admin wraps it in a card (grid) or list item (blog)
and adds the row actions. Without a block, grid and blog fall back to listing the
declared columns. Whichever presentation is chosen, scopes, filters, sorting,
pagination, and batch selection work the same way, since only the records region
changes.

## Sidebar sections

A `sidebar` declares a content panel in the right column of the index and show
pages. The block returns HTML and receives the index relation (on the index) or
the record (on the show page).

```raku
sidebar('Help', -> $context {
  '<p>Contact support for help with this resource.</p>'
});

sidebar('Recent imports', -> $relation { recent-imports-html() }, on => 'index', priority => 1);
```

`on` places the sidebar on `index`, `show`, or `both` (the default). `priority`
orders the sidebars (lower first). Pass `if-can` to gate a sidebar on an
authorization ability; it is hidden when the policy forbids that ability:

```raku
sidebar('Audit log', -> $record { audit-html($record) }, :if-can<destroy>);
```

## Panels and tabs on the show page

A `panel` adds a content panel to the main column of the show page, below the
attributes. A `tab` adds a panel to a tabbed card; declaring several tabs groups
them under tab navigation. Both receive the record and accept `priority` and
`if-can`.

```raku
panel('Notes', -> $record { notes-html($record) });

tab('Overview', -> $record { overview-html($record) });
tab('History',  -> $record { history-html($record) });
```
