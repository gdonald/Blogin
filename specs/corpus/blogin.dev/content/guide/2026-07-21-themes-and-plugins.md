---
title: Themes and Plugins
date: 2026-07-21
order: 12.5
toc: true
tags: [styling]
description: Reuse a layout set, and extend the build with a hook.
---
## Themes

A theme is a reusable set of layouts and assets. Put one under `themes/<name>/`
with its own `layouts/`, `static/`, and `assets/`, and select it in `blogin.json`:

```json
"theme": "acme"
```

The theme's files fill in behind your own, file by file. A `layouts/show.haml` in
your site overrides the theme's `show.haml` while the theme still supplies
`base.haml`, `index.haml`, and the rest. Your `static/` and `assets/` win over the
theme's on the same path, so you can replace a single stylesheet without forking
the theme.

This is distinct from a `css-framework`, which only maps element classes. A theme
brings whole templates and assets.

## Plugins

A plugin extends the build without patching Blogin. Write a module with a
`blogin-emit` hook and list it in `blogin.json`:

```json
"plugins": ["Acme::Sitemap"]
```

```raku
unit module Acme::Sitemap;

our sub blogin-emit(%context) {
  # %context<out> is the output directory, %context<pages> the rendered pages,
  # and %context<config> the site config.
  %context<out>.add('extra.txt').spurt("built { %context<pages>.elems } pages\n");
}
```

Blogin loads each named module and calls its `blogin-emit` after the build, so a
plugin can emit extra output such as a custom feed or index. With languages set,
the hook runs once per language with that language's output directory.
