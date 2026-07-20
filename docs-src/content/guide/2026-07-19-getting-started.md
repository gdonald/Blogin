---
title: Getting Started
date: 2026-07-19
tags: [intro]
description: Install Blogin and build your first site.
---
## Install

```bash
zef install Blogin
```

## Create a site

```bash
blogin init myblog
```

This scaffolds a buildable starter site: a `blogin.json`, a first post under
`content/posts/`, the layouts, and a stylesheet.

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
