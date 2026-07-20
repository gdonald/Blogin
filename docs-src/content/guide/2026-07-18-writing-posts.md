---
title: Writing Posts
date: 2026-07-18
tags: [authoring]
description: Front matter, sections, and tags.
---
A post is a Markdown file with a `---` front matter block:

```
---
title: My Post
date: 2026-07-20
tags: [raku, web]
description: A short summary.
---
The body goes here.
```

Run `blogin new "My Post"` to scaffold one with the front matter filled in.

## Sections

The subdirectory a file sits in under `content/` is its section, and the section
is both the URL prefix and the layout selector. A file at `content/posts/hello.md`
becomes `/posts/hello` and renders through `layouts/posts/show.haml` when present,
otherwise `layouts/show.haml`. Nested directories become nested sections and
nested nav entries.

## Tags

Tags are collected across every section. Each tag gets a page at `/tags/<tag>`
listing its posts, and a tag index at `/tags` lists every tag with counts.
