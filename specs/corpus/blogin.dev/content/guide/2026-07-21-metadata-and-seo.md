---
title: Metadata and SEO
date: 2026-07-21
order: 9
toc: true
tags: [seo, configuration]
description: Open Graph and Twitter tags, canonical links, and robots.txt.
---
Blogin emits per-page metadata for search engines and social cards, and a
site-wide `robots.txt`, without any template work beyond a single call in the
page head.

## head-meta

A layout's `base.haml` head calls `head-meta`, which the scaffold includes:

```haml
%head
  %title= site-title
  != head-meta
```

For a post page it emits a canonical link and the article metadata:

```html
<link rel="canonical" href="https://example.com/posts/hello"/>
<meta name="description" content="A short description."/>
<meta property="og:type" content="article"/>
<meta property="og:title" content="Hello"/>
<meta property="og:description" content="A short description."/>
<meta property="og:url" content="https://example.com/posts/hello"/>
<meta property="og:site_name" content="My Site"/>
<meta name="twitter:card" content="summary"/>
<meta name="twitter:title" content="Hello"/>
<meta name="twitter:description" content="Hello"/>
```

The description is the post's front-matter `description`, falling back to the
derived summary. The canonical and `og:url` links are absolute, built from
`base-url` in `blogin.json`, so set `base-url` for them to be complete. A listing
page emits the same shape with `og:type` of `website` and the section label as
the title.

## robots.txt

Every build writes `robots.txt` at the site root:

```
User-agent: *
Allow: /
Sitemap: https://example.com/sitemap.xml
```

The `Sitemap` line appears when `base-url` is set. Turn the file off with
`"robots": false` in `blogin.json`.
