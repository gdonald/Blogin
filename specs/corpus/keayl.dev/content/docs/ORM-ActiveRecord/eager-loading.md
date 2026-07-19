---
title: Eager loading
slug: eager-loading
order: 38
description: Accessing an association in a loop runs one query per record — the N+1 problem. Eager loading fixes it by fetching the associations up front. Three...
toc: true
---
Accessing an association in a loop runs one query per record — the N+1 problem.
Eager loading fixes it by fetching the associations up front. Three methods
differ in *how* they fetch.

## preload — a separate query per association

`preload` loads each association in its own batched query and caches the result.
No JOIN, so you can't filter on the association.

```perl6
my @users = User.where({}).preload(:pages).all;

@users[0].pages.elems;     # already loaded — no extra query
```

Without `preload`, `.pages` on each of N users would fire N queries. With it,
the pages load in one additional query and populate the per-record cache.

## eager-load — one LEFT OUTER JOIN

`eager-load` fetches everything in a single query with a `LEFT OUTER JOIN`:

```perl6
my @users = User.where({}).eager-load(:pages).all;
```

Use this when you want one round-trip, or when you need to filter on the joined
table.

## includes — preload by default, JOIN when referenced

`includes` is the one to reach for by default: it preloads (separate queries)
unless something forces a JOIN, then it switches to one.

```perl6
my @users = User.where({}).includes(:pages).all;   # behaves like preload
```

It promotes to a JOIN when you reference the association — explicitly with
`references`, or implicitly by mentioning the joined table in a `where` or
`order`:

```perl6
# explicit
User.where({}).includes(:profile).references(:profile);

# implicit: a dotted where key on the joined table
User.where({}).includes(:profile).where({'profiles.bio' => 'alice bio'});

# implicit: a nested where hash
User.where({}).includes(:profile).where({profiles => {bio => 'bob bio'}});

# implicit: ordering by the joined table
User.where({}).includes(:profile).order('profiles.bio DESC');
```

Each of these emits `LEFT OUTER JOIN profiles`, filters or orders on it, and
still caches the association. `references(profile => False)` suppresses the
promotion.

## Starting from the class

`preload`, `includes`, and `eager-load` are available directly on the model, so
you can drop the leading `where({})` when there is no condition:

```perl6
User.preload(:pages).all;
User.includes(:profile).references(:profile);
User.eager-load(:pages).all;
```

These are the same as starting from `User.where({})` or `User.all`; add a
`where` later to filter.

## Loading several associations

Pass multiple names to load them all:

```perl6
User.preload(:pages, :profile).all;
```

## Nested eager loading

Use a `Pair` to descend one level, and a hash to go deeper:

```perl6
# parent => child
User.where({}).preload(articles => :scribe).all;

# parent => { child => grandchild }
User.where({}).preload(articles => { scribe => :pages }).all;
```

```perl6
my @users = User.where({}).preload(articles => { scribe => :pages }).all;
my $article = @users[0].articles.first;
$article.scribe.pages.elems;     # all three levels loaded, no extra queries
```

This works through `:through` associations as well, caching the join collection
along the way (see [Through associations](/docs/ORM-ActiveRecord/through-associations)):

```perl6
User.where({}).preload(magazines => :subscriptions).all;
```

## Which one?

| Method | Strategy | Filter on association? |
| ------ | -------- | ---------------------- |
| `preload(:a)` | separate query | no |
| `includes(:a)` | separate query, JOIN when referenced | yes, once referenced |
| `eager-load(:a)` | one LEFT OUTER JOIN | yes |

Reach for `includes` by default; it does the right thing whether or not you end
up filtering on the association. Use `preload` to force separate queries, and
`eager-load` to force a single join.
