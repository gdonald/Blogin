---
title: Filters
slug: filters
order: 7
description: Filters are declared by hand. Each filter names a column, a type, and optionally a predicate and a collection. Nothing is inferred, and no filter ever...
toc: true
---
Filters are declared by hand. Each `filter` names a column, a type, and
optionally a predicate and a collection. Nothing is inferred, and no filter ever
enumerates a whole table into a dropdown.

```raku
filter('title',      :as<string>);                 # defaults to a substring search
filter('views',      :as<numeric>, :predicate<gteq>);
filter('published',  :as<boolean>);
filter('created-at', :as<date-range>);
filter('author-id',  :as<select>, :collection({ Author.all.map({ .id => .name }) }));
```

## Predicates

The query builder compiles a predicate onto the ORM relation, composing with the
current sort and pagination. Available predicates:

`eq`, `not-eq`, `cont`, `starts`, `ends`, `gt`, `gteq`, `lt`, `lteq`, `in`,
`between`, `present`, `blank`, `true`, `false`.

`cont`/`starts`/`ends` use SQL `LIKE` (through `ORM::ActiveRecord`'s
`LikePredicate`); the comparisons use ranges; `in` takes a comma-separated list;
`between` takes two comma-separated bounds.

Each filter type has a default predicate, overridable with `:predicate`:

| Type         | Default predicate |
| ------------ | ----------------- |
| `string`     | `cont`            |
| `numeric`    | `eq`              |
| `boolean`    | `eq`              |
| `date`       | `eq`              |
| `date-range` | `between`         |
| `select`     | `eq`              |

## Inputs

Each filter renders a type-appropriate input in the filter form: a text box for
`string`, a number box for `numeric`, an Any/Yes/No select for `boolean`, a date
picker for `date`, a pair of date pickers for `date-range`, and a dropdown for
`select`.

A `select` (or association) filter renders a dropdown only from an explicit
`collection`. Without a collection it falls back to a text search, so a large
table is never enumerated. A `select` declared with neither a `collection` nor a
search predicate is rejected at registration time.

## The filter UI

A **Filters** button in the index toolbar opens an offcanvas panel holding the
form. The button rides in the swappable index body while the panel itself is
rendered once outside it, so an `hx-get` swap never tears down an open panel.
Applying the form issues an `hx-get` that swaps the index body in place,
preserving the current sort. Active filters appear above the table as removable
chips, with a **Clear all** control. The filter values live in the query string,
so a filtered view is a shareable URL, and sort and pagination links carry the
active filters.
