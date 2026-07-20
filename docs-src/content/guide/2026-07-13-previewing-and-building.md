---
title: Previewing and Building
date: 2026-07-13
tags: [reference]
description: The dev server, incremental builds, and debug output.
---
## The preview server

```bash
blogin serve --port 3000
```

`serve` builds the site, serves `public/` locally, and watches the source tree.
Any change to content, layouts, or static assets triggers a full rebuild in
place, and config edits are picked up too. Extensionless URLs resolve to their
`.html` files, matching how a production host rewrites, so local preview behaves
like the deployed site. The server is a dev tool only; production ships static
files.

## Incremental builds

`blogin build` renders every page but writes a file only when its content
changed, so rebuilding an unchanged site writes nothing and editing one post
rewrites just that post and the pages that reference it. Output for a deleted post
is pruned. Pass `--force` to rewrite everything.

## Debug output

`blogin build --debug` injects HTML comments marking each template and partial
boundary and a provenance comment before each post body naming its source file.
It is a separate axis from `--verbose`/`--quiet`, which only affect log output.
Comment text is sanitized so a stray `-->` cannot break out.
