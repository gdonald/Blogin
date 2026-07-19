---
title: Data files
date: 2026-07-21
order: 5
toc: true
tags: [templates]
description: Load JSON and YAML into layouts, site-wide and per directory.
---
A `data/` directory beside `content/` holds structured values that layouts read
directly, so a list of authors, a set of social links, or a navigation table
lives in one place rather than being repeated across templates.

## The data tree

Every `.json`, `.yaml`, and `.yml` file under `data/` loads into a `data`
structure keyed by filename without the extension. Subdirectories nest.

```
data/
  authors.json      -> data<authors>
  site/
    links.yaml      -> data<site><links>
```

`data/authors.json`:

```json
{ "name": "Greg Donald", "url": "https://blogin.dev" }
```

A layout reaches it with a hash subscript:

```haml
%p.byline
  %a{href: "#{data<authors><url>}"}= data<authors><name>
```

JSON and YAML are interchangeable. Use whichever suits the file.

```yaml
links:
  - title: Feed
    href: /feed.xml
```

## Directory-scoped data

A `_data.json`, `_data.yaml`, or `_data.yml` file inside a content directory
applies to the pages in and beneath that directory, overriding a matching key
from the site-wide `data/` tree. Deeper directories override shallower ones.

```
content/
  posts/
    _data.json      -> merges for every page under posts/
```

Given `data/banner.json` set to `"Site banner"` and
`content/posts/_data.json` of `{ "banner": "Posts banner" }`, a post under
`posts/` reads `data<banner>` as `Posts banner`, while a page elsewhere reads
`Site banner`. Nested hashes merge key by key, so a scoped file overrides only
the keys it names and leaves the rest of the tree in place.

Section listings resolve data the same way, so `index.haml` for a section sees
that section's scoped values.
