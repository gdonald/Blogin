---
title: Deploying
date: 2026-07-11
tags: [reference]
description: Publishing the built public/ directory.
---
`blogin build` writes a self-contained `public/` directory. Deploying is copying
that directory to any static host.

## Clean URLs

With `"clean-urls": true` (the default) a post is written as
`public/posts/hello.html` and served at `/posts/hello`. The host must serve
`hello.html` for the extensionless request. This works out of the box on GitHub
Pages, Netlify, and Cloudflare Pages. On nginx, add:

```
try_files $uri $uri.html $uri/ =404;
```

Set `"clean-urls": false` to write `public/posts/hello/index.html` and serve at
`/posts/hello/` instead, which needs no host rewriting.

## Base URL

Set `base-url` in `blogin.json` to your site's origin. Feed entries, the sitemap,
and any absolute links use it, so it must match where the site is served.
