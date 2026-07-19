---
title: Feeds and Search
date: 2026-07-14
order: 8
toc: true
tags: [configuration, seo]
description: Atom feeds, the sitemap, and browser search.
---
Every build with at least one post emits feeds, a sitemap, and a search index.

## Feeds and sitemap

Blogin writes a site-wide Atom feed at `public/feed.xml` and a per-section feed at
`public/<section>/feed.xml`. Entry links are absolute, built from `base-url`, so
set that in `blogin.json`. A `public/sitemap.xml` lists every built page.

RSS 2.0 and JSON Feed are available alongside Atom. List the formats you want in
`feed-formats`:

```json
"feed-formats": ["atom", "rss", "json"]
```

Each format writes a site-wide file and a per-section file: Atom at `feed.xml`,
RSS at `rss.xml`, and JSON Feed at `feed.json`. The default is `["atom"]`.

## Search

Search runs in the browser against a prebuilt index, so production stays static.
The build writes `public/search-index.json`, one record per post with its title,
url, date, tags, description, and stripped body text (truncated to
`search-text-length`). It also emits `public/search.js`, hand-written vanilla
JavaScript that fetches the index, ranks matches (title and tag hits weigh more
than body hits), and renders results.

Matching is by prefix, so results narrow as you type: `c`, then `cs`, then `css`
each refine the same query rather than only matching a whole word.

The widget is styled out of the box. The build emits `public/search.css`
alongside the script, styling the input and rendering matches as a dropdown that
floats under the box and hides itself when a query has no results. The styling is
framework-neutral, so search looks right with or without a `css-framework`, and a
site can override any of it in its own stylesheet.

Add the form, stylesheet, and script to a page by including the `_search.haml`
partial. Turn search off with `"search": false`, and cap results with
`search-cap`.
