---
title: Getting Started
date: 2026-07-19
order: 2
toc: true
tags: [intro, cli]
description: Install Blogin and build your first site.
---
## Install Raku and zef

Blogin runs on Rakudo, the Raku compiler, and installs through zef, the Raku
module manager. If `raku` and `zef` are already on your `PATH`, skip to the next
section.

Otherwise install Rakudo Star, a bundle that ships Rakudo and zef together.
Download it for your platform from [rakudo.org](https://rakudo.org/star), or on
macOS with Homebrew:

```bash
brew install rakudo-star
```

Confirm both tools are available:

```bash
raku --version
zef --version
```

## Install Blogin

```bash
zef install Blogin
```

## Create a site

```bash
blogin init myblog
```

This scaffolds a buildable starter site: a `blogin.json`, a first post under
`content/posts/`, the layouts, and an `assets/` tree.

## Directories

| Source | Purpose |
| --- | --- |
| `content/` | Markdown posts, grouped into sections by subdirectory. |
| `layouts/` | HAML templates and partials. |
| `assets/css`, `assets/js`, `assets/img` | Stylesheets, scripts, and images, copied to `public/assets/` keeping their subfolders. This is the processed pipeline: minify, fingerprint, and responsive images apply here. |
| `static/` | Files copied to the site root verbatim, with their exact names, for `favicon.ico`, `CNAME`, `robots.txt`, `.well-known/`, and the like. Nothing here is minified or fingerprinted, so fixed URLs stay fixed. |
| `data/` | Optional JSON and YAML exposed to layouts. |
| `shortcodes/` | Optional shortcode templates. |

Blogin's own generated stylesheet and search assets are written under
`public/assets/` too, so everything an app serves lives in one place.

## Build

```bash
cd myblog
blogin build
```

The generated HTML lands in `public/`, ready to deploy to any static host.

## Preview

```bash
blogin serve
```

This builds the site, serves it locally, and rebuilds as you edit.
