---
title: Feeds and Search
date: 2026-07-14
tags: [reference]
description: Atom feeds, the sitemap, and browser search.
---
Every build with at least one post emits feeds, a sitemap, and a search index.

## Feeds and sitemap

Blogin writes a site-wide Atom feed at `public/feed.xml` and a per-section feed at
`public/<section>/feed.xml`. Entry links are absolute, built from `base-url`, so
set that in `blogin.json`. A `public/sitemap.xml` lists every built page.

## Search

Search runs in the browser against a prebuilt index, so production stays static.
The build writes `public/search-index.json`, one record per post with its title,
url, date, tags, description, and stripped body text (truncated to
`search-text-length`). It also emits `public/search.js`, hand-written vanilla
JavaScript that fetches the index, ranks matches (title and tag hits weigh more
than body hits), and renders results.

Add the form and script to a page by including the `_search.haml` partial. Turn
search off with `"search": false`, and cap results with `search-cap`.
