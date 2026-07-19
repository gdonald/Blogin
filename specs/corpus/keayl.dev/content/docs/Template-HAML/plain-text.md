---
title: Plain text
slug: plain-text
order: 8
description: Any line whose first non-space character is not a HAML sigil is plain text. Plain text is emitted as-is at the current indent level.
toc: true
---
Any line whose first non-space character is not a HAML sigil is plain text.
Plain text is emitted as-is at the current indent level.

```haml
%div
  Hello world
  Another line
```

renders to:

```html
<div>
  Hello world
  Another line
</div>
```

## Backslash escape

To produce text that begins with a HAML sigil, prefix the line with `\`. The
backslash is stripped from the output.

```haml
\%foo
\.bar
\#hash
```

renders to:

```html
%foo
.bar
#hash
```

## Interpolation

`#{ ... }` evaluates a Raku expression, HTML-escapes the result, and emits it.

```haml
%p Hello, #{ $name }!
```

with `:locals(%(name => 'world'))` renders to:

```html
<p>Hello, world!</p>
```

`!{ ... }` evaluates and emits the result *without* HTML-escaping. Use it when
the value is already known-safe HTML.

```haml
%p !{ $raw }
```

with `:locals(%(raw => '<b>bold</b>'))` renders to:

```html
<p><b>bold</b></p>
```

To produce a literal `#{...}` or `!{...}`, escape the leading sigil with a
backslash:

```haml
%p \#{ literal }
```

renders to:

```html
<p>#{ literal }</p>
```

Interpolation is recognized in tag content, plain text lines, and inside
double-quoted attribute strings:

```haml
%a{ href: "/users/#{ $id }" } Profile
```

with `:locals(%(id => 7))` renders to:

```html
<a href='/users/7'>Profile</a>
```

Single-quoted attribute strings are literal — no interpolation is performed:

```haml
%a{ title: 'Hello #{name}' } go
```

renders to:

```html
<a title='Hello #{name}'>go</a>
```

## Escaping rules

Inside `#{ ... }`, the value is HTML-escaped before emission, so user input
cannot inject markup:

```haml
%p #{ $tag }
```

with `:locals(%(tag => '<b>'))` renders to:

```html
<p>&lt;b&gt;</p>
```

Inside `!{ ... }`, the value is emitted raw. Inside attribute values, the
final attribute value is escaped once after interpolation, so `#{}` and
`!{}` produce equivalent output for attributes.

## Multi-line content

The legacy `|` line-continuation operator from Ruby HAML 3 is **not**
supported. Wrap multi-line content in a tag or use embedded Raku for
formatting:

```haml
%p
  First line
  Second line
```
