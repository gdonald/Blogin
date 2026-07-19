---
title: Assets
slug: assets
order: 19
description: The admin ships a vendored frontend bundle and serves it through the engine, so it works with no external requests and no CDN dependency.
toc: true
---
The admin ships a vendored frontend bundle and serves it through the engine, so
it works with no external requests and no CDN dependency.

## The bundle

| Component       | Version | Logical paths                                                      |
| --------------- | ------- | ------------------------------------------------------------------ |
| Bootstrap       | 5.3.3   | `bootstrap/bootstrap.min.css`, `bootstrap/bootstrap.bundle.min.js` |
| Bootstrap Icons | 1.11.3  | `bootstrap-icons/bootstrap-icons.min.css`                          |
| htmx            | 2.0.4   | `htmx/htmx.min.js`                                                 |

The files ship as distribution resources, so they resolve whether the admin runs
from a source checkout or an install. They are pinned in
`tools/vendored-assets.json` (versions, source URLs, and checksums); run
`raku tools/vendor-assets.raku` to refetch or upgrade them.

## Serving and fingerprinting

The engine serves the bundle under `<mount>/admin-assets/<path>`. Each URL
carries a content digest as a query string, and responses are sent with
`Cache-Control: public, max-age=31536000, immutable`. When a file's content
changes its digest changes, so the URL changes and caches refresh on their own.

`MVC::Keayl::Admin::Assets.asset-url('bootstrap/bootstrap.min.css')` returns the
fingerprinted URL. The layout renders the stylesheet, script, and import-map
tags for the bundle. htmx and Bootstrap are also pinned in an import map.

## Overriding styles

A host application can layer its own stylesheet after the bundle:

```raku
MVC::Keayl::Admin.use-stylesheet('/assets/admin-overrides.css');
```

The override is emitted after the vendored stylesheets, so its rules win.

## Licensing

The vendored assets are separate works under their own licenses (Bootstrap and
Bootstrap Icons are MIT, htmx is Zero-Clause BSD), distinct from this
distribution's Artistic-2.0 license. Each component's `LICENSE` is bundled next
to its files, and [NOTICES.md](https://github.com/gdonald/MVC-Keayl-Admin/blob/main/NOTICES.md)
records the versions, licenses, and sources.
