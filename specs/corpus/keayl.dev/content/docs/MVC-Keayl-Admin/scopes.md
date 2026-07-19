---
title: Scopes
slug: scopes
order: 8
description: A scope is a named view over the resource, declared with a block that is applied to the base relation. One scope can be marked :default.
toc: true
---
A scope is a named view over the resource, declared with a block that is applied
to the base relation. One scope can be marked `:default`.

```raku
scope('All', :default);
scope('Published', { .where({ published => True }) });
scope('Drafts',    { .where({ published => False }) });
```

The block receives the relation and returns a narrowed one, so it composes with
the ORM like any other query step.

## Resolution

The active scope comes from the query string (`?scope=Published`), falling back
to the `:default` scope when none is given or the name is unknown. The scope is
applied before filters and sort, and the filtered, scoped count drives
pagination.

## Scope tabs

Scopes render as tabs above the table, with the active one highlighted. Each tab
carries the current filters and sort, and switching scopes swaps the index body
over HTMX. Sort, pagination, and filter links carry the active scope in turn, so
the three compose.

## Per-scope counts

By default each tab shows a count, computed by running the scope (with the
current filters) through a count query. For an expensive data set, suppress the
counts per resource:

```raku
MVC::Keayl::Admin.register(Post, { ... }, scope-counts => False);
```
