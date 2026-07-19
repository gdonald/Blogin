---
title: Previewing and Building
date: 2026-07-13
order: 10
toc: true
tags: [cli, builds]
description: The dev server, incremental builds, and debug output.
---
## The preview server

```bash
blogin serve --port 3000
```

`serve` builds the site, serves `public/` locally, and watches the source tree.
Any change to content, layouts, or static assets triggers a full rebuild in
place, and config edits are picked up too. Every directory under the watched
trees is watched, however deeply nested, and a directory you add while the server
runs starts firing changes on its own. A save that fires several filesystem
events is coalesced into one rebuild. Extensionless URLs resolve to their `.html`
files, matching how a production host rewrites, so local preview behaves like the
deployed site. The server is a dev tool only; production ships static files.

`minify` and `fingerprint` are off while serving, whatever `blogin.json` says,
and the server prints a line saying so when the config asks for them.
Fingerprinting renames every asset on each build, which would leave an already
open page pointing at files the rebuild just deleted, and it costs that renaming
work on every save. `blogin build` still applies both, so what you deploy is
unaffected.

## Live reload

Each served HTML page carries a small client that holds a WebSocket to the
server. When a rebuild finishes, the server pushes the list of output files it
actually rewrote, and the page acts on it:

- The page reloads when its own HTML file changed, or when a script it loads
  changed.
- A changed stylesheet is swapped in place, so styling updates without a reload
  and without losing scroll position or form state.
- A changed image is re-fetched in place.
- A rebuild that rewrote nothing sends nothing, and no page reloads.

The client reconnects on its own when the socket drops, so a restarted server
picks the page back up. On reconnecting it compares the build the page is showing
against the one the server is serving, and reloads when they differ, which covers
edits made while the server was down. Preview responses are sent with
`Cache-Control: no-store` so a reloaded page never comes back from the browser
cache.

The injection happens only while serving. Built `public/` files never contain it.

## Incremental builds

`blogin build` renders every page but writes a file only when its content
changed, so rebuilding an unchanged site writes nothing and editing one post
rewrites just that post and the pages that reference it. Output for a deleted post
is pruned. Pass `--force` to rewrite everything.

A `.keep` file is yours, not build output, so pruning leaves it alone and the
directory holding it survives. Keep one at `public/.keep` to track the output
directory in git, or in any directory under `public/` the build writes nothing
into. `blogin clean` leaves `.keep` files and their directories in place too.

## Asset optimization

Two `blogin.json` keys tune the emitted assets, and both act only on the assets
pipeline under `public/assets/`. Files copied verbatim from `static/` to the site
root are never touched, so a `favicon.ico` or `CNAME` keeps its exact name.

`minify` shrinks every CSS and JavaScript file under `assets/`: it strips
comments, collapses whitespace, and drops blank and comment-only script lines
while keeping line breaks so JavaScript semicolon insertion stays safe.

`fingerprint` renames each CSS, JavaScript, and image file under `assets/` to
include a short hash of its size and timestamp, such as `style.1a2b3c4d.css`, and
rewrites every reference to it in the built HTML and CSS. A changed asset gets a
new name, so a far-future cache never serves a stale file. Both default off.

`image-widths` turns on responsive images. Given a list like `[320, 640, 960]`,
the build resizes each raster image to every width smaller than the original,
writing variants like `photo-320.png` beside it, and adds a `srcset` to each
reference so the browser picks the right size. It needs an image resizer on the
build host: ImageMagick or, on macOS, `sips`. With no widths set, images are
copied unchanged.

## Debug output

`blogin build --debug` injects HTML comments marking each template and partial
boundary and a provenance comment before each post body naming its source file.
It is a separate axis from `--verbose`/`--quiet`, which only affect log output.
Comment text is sanitized so a stray `-->` cannot break out.
