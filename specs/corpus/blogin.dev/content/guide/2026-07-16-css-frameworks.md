---
title: CSS Frameworks
date: 2026-07-16
order: 6
toc: true
tags: [styling]
description: Emit framework-specific classes without touching the core renderer.
---
The HTML Blogin emits carries framework-specific classes drawn from a selected
profile, set by the `css-framework` config key. The core renderer stays
framework-agnostic: it asks the profile for a class rather than hardcoding one.

## Profiles

| Profile | Style |
| --- | --- |
| `none` | Plain semantic HTML, no extra classes (default). |
| `bootstrap5` | Per-element classes (`table`, `blockquote`, `img-fluid`, `pagination`). |
| `pico` | Classless; styles bare semantic HTML. |
| `bulma` | Class-based, with a `.content` wrapper and `.pagination`/`.navbar`/`.tag`. |

Under `none` and `pico` the elements stay unclassed. Under `bootstrap5` and
`bulma` the renderer adds each framework's classes to the same semantic HTML, so
switching frameworks never changes the markup structure.

## Wiring the assets

`blogin init --framework=bootstrap5` (or `pico`, `bulma`) records the framework in
`blogin.json`. The layout emits that framework's assets from the config through two
helpers:

- `framework-stylesheet-tag` renders the stylesheet `<link>` in the document head.
- `framework-script-tag` renders the JavaScript bundle `<script>` before the
  closing `</body>`.

Both read the selected profile, so changing `css-framework` in `blogin.json`
swaps the linked assets without editing any template. A profile with no script
(the `none`, `pico`, and `bulma` profiles) renders an empty script tag, so the
bundle only loads where a framework ships one.

Bootstrap 5 ships `bootstrap.bundle.min.js`, which powers its interactive
components (the responsive navbar toggle, dropdowns, collapses). The scaffolded
`base.haml` includes `framework-script-tag`, so those components work out of the
box.

Layouts read a slot's class with `framework-class('table')`, so a
framework-aware template picks up the right class automatically. The built-in
`pagination-html` uses this to class its markup per framework, so a paginated
listing looks right under each profile with no layout change.
