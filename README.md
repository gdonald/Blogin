# Blogin

A static blog generator written in Raku. Point it at a directory of Markdown
files and it produces a directory of plain HTML you can host anywhere. No
database, no runtime: only the build host needs Rakudo, and what ships is static
HTML, CSS, and one search script.

## Install

```bash
zef install Blogin
```

## Quick start

```bash
blogin init myblog     # scaffold a buildable starter site
cd myblog
blogin new "Hello"     # add a post
blogin serve           # preview at http://localhost:3000, rebuilds on save
blogin build           # write public/ for deployment
```

## Features

- A Raku-owned Markdown parser (CommonMark subset plus tables, task lists,
  strikethrough, autolinks, and link/image attribute lists) rendering to a typed
  AST, then HTML and stripped plain text in one pass.
- HAML layouts via Template::HAML, with per-section layout resolution and shared
  header/sidebar/footer chrome partials.
- Sections derived from the content tree, extensionless URLs, a recursive
  navigation menu, paginated listings, and tag pages.
- Atom feeds (site-wide and per-section), a sitemap, and browser search against a
  prebuilt index.
- CSS-framework profiles (none, Bootstrap 5, Pico, Bulma) that add classes to the
  same semantic HTML without touching the renderer.
- Server-side syntax highlighting, a Cro-backed preview server, and content-hash
  incremental builds.

## Documentation

The documentation site is itself a Blogin instance under `docs-src/`. Build it
with `blogin build` from that directory.

## Development

Run the test suite with `raku test.raku` (BDD::Behave specs under `specs/`).

## License

Artistic-2.0
