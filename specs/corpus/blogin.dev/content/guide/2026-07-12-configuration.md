---
title: Configuration
date: 2026-07-12
order: 11
tags: [configuration]
description: The keys in blogin.json.
---
Site-wide settings live in `blogin.json` at the project root. Command-line
options override the file.

| Key | Meaning |
| --- | --- |
| `title` | Site title, available to layouts. |
| `base-url` | Absolute base for feeds and the sitemap. |
| `author` | Site author, available to layouts and feeds. |
| `output-dir` | Where the build writes (default `public`). |
| `home-section` | Section whose listing is also the site root. |
| `clean-urls` | Extensionless URLs when true (default false; true needs web-server config, see [Deploying](/guide/deploying/)). |
| `css-framework` | Class-map profile: `none`, `bootstrap5`, ... |
| `page-size` | Posts per listing page. |
| `summary-length` | Character cap on an auto-generated post summary (default 200). |
| `robots` | Emit `robots.txt` (default true). |
| `minify` | Minify CSS and JavaScript under `assets/` (default false). `static/` files are left verbatim. |
| `fingerprint` | Hash `assets/` filenames and rewrite references (default false). `static/` files keep their names. |
| `image-widths` | Widths to resize `assets/` images to for `srcset` (default `[]`, off). |
| `reading-wpm` | Words per minute for the reading-time estimate (default 200). |
| `related-count` | Maximum related posts per post (default 5). |
| `languages` | Language codes to build, each into its own `/<code>/` subtree. |
| `language-config` | Per-language overrides, e.g. `title`, keyed by code. |
| `theme` | Name of a theme under `themes/` to fall back to for layouts and assets. |
| `plugins` | Module names whose `blogin-emit` hook runs after the build. |
| `taxonomies` | Front-matter keys to group posts by (default `["tags"]`). |
| `feed-formats` | Feed formats to emit: `atom`, `rss`, `json` (default `["atom"]`). |
| `highlight` | Server-side syntax highlighting for fenced code. |
| `search` | Emit the search index and script. |
| `search-text-length` | Characters of body text indexed per post (default 2000). |
| `search-cap` | Maximum search results shown (default 10). |
| `debug` | Emit provenance comments around rendered partials and pages. |

## Per-section overrides

The `sections` map overrides settings for one section, including its nav label,
nav order, visibility, page size, and whether dates show:

```
"sections": {
  "guide": { "label": "Guide", "order": 1, "page-size": 20 }
}
```

| Section key | Meaning |
| --- | --- |
| `label` | Nav label and listing heading (defaults to the humanized name). |
| `order` | Sort position in the nav. |
| `page-size` | Posts per listing page for this section. |
| `nav` | Include the section in the nav when true. |
| `layout` | Layout name override for the section. |
| `index-dates` | Show post dates on the section's listing pages (default true). |
| `show-dates` | Show the post date on the section's post pages (default true). |

Set `index-dates` or `show-dates` to `false` to hide dates on reference-style
sections while a blog keeps them.
