---
title: Deploying
date: 2026-07-11
order: 13
toc: true
tags: [builds]
description: Publishing the built public/ directory.
---
`blogin build` writes a self-contained `public/` directory. Deploying is copying
that directory to any static host.

## Clean URLs

By default (`"clean-urls": false`) a post is written as
`public/posts/hello/index.html` and served at `/posts/hello/`. Every static host
serves an `index.html` for a directory request, so this works everywhere with no
configuration.

Set `"clean-urls": true` for extensionless URLs: the post is written as
`public/posts/hello.html` and linked as `/posts/hello`. The host then has to serve
`hello.html` for the extensionless request, which needs server configuration.
Managed hosts do this for you: GitHub Pages, Netlify, and Cloudflare Pages serve
`/posts/hello` from `posts/hello.html` out of the box.

On a self-managed server you configure the rewrite yourself. For nginx, in the
`server` block:

```
location / {
  try_files $uri $uri.html $uri/ =404;
}
```

For Apache, in a `.htaccess` at the site root:

```
RewriteEngine On
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME}\.html -f
RewriteRule ^(.+)$ $1.html [L]
```

If you would rather not touch the server, leave `clean-urls` off.

## Base URL

Set `base-url` in `blogin.json` to your site's origin. Feed entries, the sitemap,
and any absolute links use it, so it must match where the site is served.
