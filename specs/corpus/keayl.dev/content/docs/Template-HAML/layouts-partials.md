---
title: Layouts and partials
slug: layouts-partials
order: 13
description: Template::HAML supports rendering templates from disk, composing them into layouts, and re-using fragments via partials. These features build on a small...
toc: true
---
Template::HAML supports rendering templates from disk, composing them into
layouts, and re-using fragments via partials. These features build on a
small instance API on `HAML`:

```raku
use Template::HAML;

my $haml = HAML.new(:search-paths('app/views'));
say $haml.render(:file<home/index>, :layout<layouts/app>);
```

## Search paths

`HAML.new(:@search-paths)` configures the directories that are searched
when resolving a template by name. The list is ordered: the first match
wins.

```raku
my $haml = HAML.new(:search-paths('app/views', 'shared/views'));
```

A single string is accepted as a convenience for the common one-path
case:

```raku
my $haml = HAML.new(:search-paths('app/views'));
```

When no search paths are given the current working directory is used.

## File rendering

```raku
$haml.render(:file<home/index>);
```

The template name is resolved against every search path, trying the
following filename variants in order:

1. exact name (`home/index`)
2. `<name>.haml`
3. `<name>.html.haml`
4. partial form: `<dir>/_<basename>` (and its `.haml` / `.html.haml` variants)

`$haml.render(:file<...>)` looks up and renders one file. `$haml.render(:src<...>)`
still works for inline source. Absolute paths are honored as-is.

A missing template raises `X::HAML::TemplateNotFound`, which carries the
attempted name and the configured search paths.

## Layouts and `yield`

A layout is a template that contains `= yield` somewhere in its body.
When you call `render(:file, :layout)`, the inner template is rendered
first; its output is then substituted for `yield` in the layout.

`app/views/layouts/app.html.haml`:

```haml
%html
  %body
    = yield
```

`app/views/home/index.html.haml`:

```haml
%h1 Hello
```

```raku
$haml.render(:file<home/index>, :layout<layouts/app>);
```

renders:

```html
<html>
  <body>
    <h1>Hello</h1>

  </body>
</html>
```

The same call works with the compiled cache via `render-file-cached`:

```raku
$haml.render-file-cached(:file<home/index>, :layout<layouts/app>);
```

Both the inner template and the layout are compiled, cached on disk, and
memoized in-process. See the API reference for details.

### Named content blocks

Inner templates can fill *named* slots in a layout with `content-for`.
The layout retrieves them via `yield(:name<...>)`:

Inner:

```haml
- content-for('title', { 'My Page' })
%p body
```

Layout:

```haml
%html
  %head
    %title
      != yield(:name<title>)
  %body
    = yield
```

`yield` and `content-for` are exported by `Template::HAML::Helpers` and are
visible to every embedded expression. `yield` returns a `SafeString`, so
its result is not double-escaped by the renderer.

## Partials

A partial is a fragment that is rendered inline from another template.
By convention partial filenames begin with an underscore (`_header.haml`);
the leading underscore is added automatically by the lookup logic, so the
call site just names the partial:

`_header.html.haml`:

```haml
%h1 Welcome
```

In a parent template:

```haml
%div
  != render(:partial<header>)
```

### Locals

Pass per-partial locals through `:locals`:

```haml
!= render(:partial<greeting>, :locals(%(name => 'World')))
```

Inside `_greeting.haml`:

```haml
%p Hello, #{$name}!
```

### Collections

`:collection` renders the partial once per element. The element is
exposed under the name given by `:as` (defaults to `item`):

```haml
%ul
  != render(:partial<row>, :collection(@items), :as<row>)
```

Inside `_row.haml`:

```haml
%li #{$row.name}
```

### Recursion limit

Partials may recurse, but the depth is bounded by the rendering context
(default 100). Exceeding the limit raises
`X::HAML::PartialDepthExceeded`.

## Compilation cache

Templates loaded by `compile-file` (used internally by `:file` and
`:partial` lookups) are cached per `HAML` instance, keyed on absolute
path and `mtime`. Edits to a template are picked up automatically on the
next render.

```raku
my $haml = HAML.new(:search-paths('app/views'));        # cache on
my $dev  = HAML.new(:search-paths('app/views'), :!cache); # cache off
$haml.clear-cache;
```
