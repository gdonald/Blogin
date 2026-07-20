---
title: CSS Frameworks
date: 2026-07-16
tags: [reference]
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

## Wiring the stylesheet

`blogin init --framework=bootstrap5` (or `pico`, `bulma`) records the framework in
`blogin.json` and links that framework's stylesheet in `base.haml`. Layouts read
a slot's class with `framework-class('pagination')`, so a framework-aware template
picks up the right class automatically.
