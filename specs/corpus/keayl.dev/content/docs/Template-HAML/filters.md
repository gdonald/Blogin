---
title: Filters
slug: filters
order: 12
description: A filter line begins with : followed by the filter name. The filter's body is the indented block beneath it. Each filter passes its body through a handler...
toc: true
---
A filter line begins with `:` followed by the filter name. The filter's body
is the indented block beneath it. Each filter passes its body through a
handler that returns rendered text.

```haml
:javascript
  alert('hi');
```

renders to:

```html
<script>
  alert('hi');
</script>
```

## Built-in filters

### `:plain`

Emits the body verbatim with no sigil parsing and no HTML escaping.
Interpolates `#{ ... }` expressions.

```haml
:plain
  Hello, #{$name}!
  <b>raw html stays raw</b>
```

### `:escaped`

HTML-escapes the body (after interpolation), then emits.

```haml
:escaped
  <b>raw</b> & "stuff"
```

renders to:

```html
&lt;b&gt;raw&lt;/b&gt; &amp; &quot;stuff&quot;
```

### `:javascript`

Wraps the body in `<script> ... </script>`. Interpolates `#{ ... }`.

When the format is `xhtml`, or when the `cdata` config option is `True`, the
body is additionally wrapped in `//<![CDATA[ ... //]]>` markers and the
`<script>` tag emits `type="text/javascript"` (or whatever `mime-type` is set
to). In HTML5 with `mime-type` set, the type attribute is emitted with no
CDATA wrapping unless `cdata: True`.

### `:css`

Wraps the body in `<style> ... </style>`. Interpolates `#{ ... }`.

`:css` mirrors `:javascript` for the `cdata` / `mime-type` config options:
the CDATA markers use the `/*<![CDATA[*/ ... /*]]>*/` form, and the default
XHTML type attribute is `text/css`.

### `:cdata`

Wraps the body in `<![CDATA[ ... ]]>`. Interpolates `#{ ... }`.

### `:preserve`

Replaces newlines in the body with `&#x000A;` so the output stays on a single
line. Useful for preserving whitespace inside a tag without inflating the
parent indentation. Interpolates `#{ ... }`.

```haml
:preserve
  line1
  line2
```

renders to:

```html
line1&#x000A;line2
```

### `:markdown`

Renders the body as Markdown by delegating to a user-registered backend.
Interpolates `#{ ... }` before the body is passed to the backend.

```haml
:markdown
  # Hello, #{$name}!
  body text
```

`Template::HAML` ships **no** Markdown engine of its own — Raku has several
viable Markdown libraries and the right choice belongs to the host
application. Register one with `register-markdown-backend`:

```raku
use Template::HAML;
use Template::HAML::Filters;
use Text::Markdown;  # or any other Markdown engine

register-markdown-backend :handler(-> Str $body --> Str {
  Text::Markdown.new(:text($body)).render;
});
```

The handler signature is `(Str $body --> Str)`. The interpolated body is
passed in; the returned string is the rendered HTML. The renderer prefixes
each output line with the filter's own source indent, so a `:markdown` nested
inside `%div` lines up correctly.

If `:markdown` appears in a template and no backend is registered, render
fails with `X::HAML::MarkdownBackendMissing`. Related helpers:

| Sub                            | What it does                                          |
| ------------------------------ | ----------------------------------------------------- |
| `register-markdown-backend`    | Install or replace the backend handler.               |
| `clear-markdown-backend`       | Remove the current handler. Returns the previous defined-ness as a `Bool`. |
| `markdown-backend-registered`  | `True` if a backend is currently installed.           |
| `current-markdown-backend`     | The current handler, or an undefined `Callable`.      |

Calling `register-markdown-backend` again replaces the previous handler — at
most one Markdown backend is active per process.

### `:raku`

Evaluates the body as Raku code with the current locals in scope and emits the
return value as a string. The body is **not** interpolated — it *is* code.

```haml
:raku
  $x * 2
```

with `:locals(%(x => 10))` renders to:

```html
20
```

## Custom filters

Register a filter via `register-filter`:

```raku
use Template::HAML;
use Template::HAML::Filters;

register-filter :name<upper>, :handler(-> Str $body, %locals --> Str {
  $body.uc;
});

say HAML.render(:src(":upper\n  hello\n"));
# HELLO
```

The handler signature is `(Str $body, %locals --> Str)`. The `$body` is the
filter's indented block dedented to column zero; the handler returns the
rendered text. The renderer prefixes each output line with the filter's own
source indent.

## Interpolation summary

| Filter        | Interpolates `#{ ... }` |
| ------------- | ----------------------- |
| `:plain`      | yes                     |
| `:escaped`    | yes                     |
| `:javascript` | yes                     |
| `:css`        | yes                     |
| `:cdata`      | yes                     |
| `:preserve`   | yes                     |
| `:markdown`   | yes                     |
| `:raku`       | no (body is code)       |

## Indentation

Body lines must be indented strictly more than the filter line. The minimum
indent across all non-blank body lines is stripped, so relative indentation
inside the body is preserved:

```haml
:plain
  outer
    inner
```

renders to:

```
outer
  inner
```

Internal blank lines inside the body are preserved. Trailing blank lines
between the filter body and the next sibling are stripped.

## Errors

An unknown filter name raises `X::HAML::UnknownFilter` at render time, with
the offending name and source position.
