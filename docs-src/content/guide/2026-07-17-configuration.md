---
title: Configuration
date: 2026-07-17
tags: [reference]
description: The keys in blogin.json.
---
Site-wide settings live in `blogin.json` at the project root. Command-line
options override the file.

| Key | Meaning |
| --- | --- |
| `title` | Site title, available to layouts. |
| `base-url` | Absolute base for feeds and the sitemap. |
| `output-dir` | Where the build writes (default `public`). |
| `home-section` | Section whose listing is also the site root. |
| `clean-urls` | Extensionless URLs when true. |
| `css-framework` | Class-map profile: `none`, `bootstrap5`, ... |
| `page-size` | Posts per listing page. |
| `highlight` | Server-side syntax highlighting for fenced code. |
| `search` | Emit the search index and script. |

## Per-section overrides

The `sections` map overrides settings for one section, including its nav label,
nav order, visibility, and page size:

```
"sections": {
  "guide": { "label": "Guide", "order": 1, "page-size": 20 }
}
```
