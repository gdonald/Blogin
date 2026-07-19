---
title: Template Data
date: 2026-07-22
order: 4.5
toc: true
tags: [templates]
description: The context object every layout renders against, and the shapes it exposes.
---
Every layout renders against a context object. A method call in HAML is a call on
that object, so `= title` runs the context's `title` method and `- for posts`
iterates its `posts` collection. There are two context types. A post context
backs `show.haml`, and a listing context backs `index.haml`, `home.haml`,
`tag.haml`, `tags.haml`, and `404.haml`. Both build on a shared site context, and
`base.haml` sees whichever one wraps the page, so site-wide methods work in the
shell and in every template it yields into.

Knowing the exact surface is what lets you customize past the scaffold. This page
lists every method and collection each context exposes, and the shape of the data
inside the collections.

## The site context

These are available in every template, including `base.haml` and the partials.

| Method | Returns |
| --- | --- |
| `site-title` | The configured `title`. |
| `section` | The current section path, `''` at the root. |
| `section-label` | The section's nav label, or its humanized name. |
| `url` | The page's own URL. |
| `canonical-url` | `base-url` joined to `url`, for a canonical link. |
| `head-meta` | The Open Graph, Twitter, and canonical tags as one HTML string. |
| `meta-title` | The document title for the `<title>` tag: the site title, then the page's own title after a ` :: ` separator (`Greg Donald :: My Post`). The home page is the site title alone. |
| `page-title` | The page's own title without the site prefix: the post title, the section label, or a term name. Empty on the home page. This is what `head-meta` puts in `og:title` and `twitter:title`. |
| `meta-description`, `meta-type` | The other values `head-meta` uses, each callable on its own. |
| `data` | The merged data files as a hash, keyed by file name. See [Data files](/guide/data-files/). |
| `languages`, `has-languages` | The translations of the current page, and whether there are any. |
| `nav-nodes` | The navigation tree, a list of nodes. |
| `nav-current(node)` | True when a nav node is the current section or an ancestor of it. |
| `framework-class(slot)` | The CSS-framework class for a named slot, `''` under `none`. |
| `framework-stylesheet-tag`, `framework-script-tag` | The framework's `<link>` and `<script>`, `''` under `none`. |
| `theme-script` | An inline `<script>` for the `<head>` that applies the saved or preferred color theme before paint and defines the toggle. |
| `theme-toggle` | A ready-made light/dark toggle button, moon and sun icons, wired to the script. |
| `has-header`, `has-sidebar`, `has-footer` | Whether the matching chrome partial exists. |
| `truncate(text, n)`, `format-date(date, fmt)`, `group-by(items, field)` | Filters. See [Layouts](/guide/layouts/). |
| `cache-fragment(key, block)` | Render the block once and reuse its HTML for every later call with the same key. For invariant chrome only. See [Layouts](/guide/layouts/). |

## The post context

`show.haml` adds these to the site context.

| Method | Returns |
| --- | --- |
| `title`, `date`, `description` | The post's front matter fields. |
| `show-dates` | Whether the section shows dates. |
| `body` | The rendered post HTML. Insert with `!=`, it is already escaped where it needs to be. |
| `has-toc`, `toc-html` | Whether `toc: true` is set, and the table of contents as HTML. |
| `word-count`, `reading-time` | The body word count and its whole-minute reading time. |
| `related`, `has-related` | Posts sharing taxonomy terms, most-shared first, as post entries. |
| `tags`, `has-tags` | The post's own tags as `{ name, url }` links, and whether there are any. |
| `post-nav-html` | The newer/older links within the section, as HTML. |

A `show.haml` that uses the lot:

```haml
%article
  %h1= title
  - if show-dates
    %p.meta= "#{date} · #{reading-time} min read"
  - if has-toc
    %nav.toc
      != toc-html
  != body
  - if has-tags
    %nav.tags
      - for tags -> $tag
        %a{href: "#{$tag<url>}"}= $tag<name>
!= post-nav-html
- if has-related
  %nav.related
    %h2 Related
    != render(:partial<entry>, :collection(related), :as<entry>)
```

## The listing context

`index.haml`, `home.haml`, `tag.haml`, `tags.haml`, and `404.haml` add these.

| Method | Returns |
| --- | --- |
| `heading` | The page's `h1` text: the section label, or the term name on a term page, or the humanized taxonomy name on a taxonomy index. |
| `posts` | The entries to list. |
| `page-number`, `total-pages` | The current page and the page count. |
| `pagination-html` | A numbered pagination bar as HTML: first, previous, the current page with three on either side, next, and last. Classed for the active CSS framework. Empty on a single-page listing. |
| `pagination-links` | One entry per page, for a layout that renders its own pagination instead. Empty on a single-page listing. |
| `index-dates` | Whether the section lists dates. |

`posts` holds different shapes depending on the page. On a section listing,
`home.haml`, and a term page it holds post entries. On a taxonomy index such as
`tags.haml` it holds term entries. A section listing:

```haml
%section
  %h1= heading
  - for posts -> $entry
    %article
      %a{href: "#{$entry<url>}"}= $entry<title>
      - if index-dates
        %span.date= $entry<date>
      %p= $entry<description>
  != pagination-html
```

The tag index, iterating term entries into a cloud:

```haml
%section
  %h1= heading
  - for posts -> $term
    %a.tag{href: "#{$term<url>}"}= $term<title>
```

## The shapes inside the collections

The collections hold plain hashes and nodes with a fixed set of keys.

A **post entry** (`posts` on a listing, `related` on a post):

```
{ title, url, date, description, summary, image }
```

`image` is the source of the first image in the post's body, or `''` when the
post has none. A listing renders it as a lead thumbnail with `- if $entry<image>`.

A **term entry** (`posts` on a taxonomy index). Its `title` already carries the
count, such as `raku (4)`:

```
{ title, url, date }
```

A **tag link** (`tags` on a post):

```
{ name, url }
```

A **pagination link** (`pagination-links` on a listing). `current` marks the page
being rendered:

```
{ number, url, current }
```

A layout builds numbered pagination by iterating them:

```haml
- if pagination-links
  %nav.pagination
    - for pagination-links -> $page
      - if $page<current>
        %span.current= $page<number>
      - else
        %a{href: "#{$page<url>}"}= $page<number>
```

A **language** (`languages`). `current` marks the page you are on:

```
{ code, url, current }
```

A **nav node** (`nav-nodes`, and each node's `children`) is an object, so read it
with method calls, not hash keys:

```haml
%a{href: "#{$node.url}"}= $node.label
- if $node.children.elems
  %ul
    != render(:partial<nav-item>, :collection($node.children), :as<node>)
```

Its fields are `name`, `label`, `path`, `url`, `order`, and `children`.

## Partials and locals

A partial renders against the same context as the template that calls it, so
every method above is in scope. Two things pass extra values in. A collection
render binds each item to the name in `:as`:

```haml
!= render(:partial<entry>, :collection(posts), :as<entry>)
```

Inside `_entry.haml`, `$entry` is one post entry. Explicit locals pass a hash:

```haml
!= render(:partial<header>, :locals(%( brand => site-title )))
```

Inside `_header.haml`, `$brand` is that value. Locals and the `:as` binding arrive
as `$`-sigiled variables (`$entry`, `$node`, `$brand`), which is what sets them
apart from the context methods, which are barewords.
