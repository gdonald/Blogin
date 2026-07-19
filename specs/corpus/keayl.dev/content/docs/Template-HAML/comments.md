---
title: Comments
slug: comments
order: 7
description: Template::HAML supports three forms of comments: HTML comments, conditional comments, and silent comments.
toc: true
---
Template::HAML supports three forms of comments: HTML comments, conditional comments, and silent comments.

## HTML comments — `/`

A line beginning with `/` is rendered as an HTML comment.

Single-line form:

```haml
/ Hello
```

```html
<!-- Hello -->
```

If the `/` line has children indented underneath, they are rendered inside the comment block:

```haml
/
  %p hi
```

```html
<!--
  <p>hi</p>
-->
```

## Conditional comments — `/[expr]`

Use `/[expr]` to emit a conditional comment. The bracketed text becomes the condition; surrounding whitespace is stripped.

Inline form:

```haml
/[if IE] text
```

```html
<!--[if IE]>text<![endif]-->
```

Block form:

```haml
/[if IE]
  %p ie only
```

```html
<!--[if IE]>
  <p>ie only</p>
<![endif]-->
```

Negated:

```haml
/[if !IE] text
```

```html
<!--[if !IE]>text<![endif]-->
```

### Revealed conditional comments — `/![expr]`

Prefix the bracket with `!` to emit a downlevel-revealed conditional comment. Browsers that don't honor the condition see the wrapped content as plain markup:

```haml
/![if !IE] text
```

```html
<!--[if !IE]><!-->text<!--<![endif]-->
```

## Silent comments — `-#`

A line beginning with `-#` is suppressed entirely. Any indented body — including any number of blank lines — is treated as opaque text and produces no output:

```haml
%p hi
-# notes:
   arbitrary text $^&* not parsed as HAML
   !!! also opaque
%p bye
```

```html
<p>hi</p>
<p>bye</p>
```

Because the body is opaque, you can drop in non-HAML content (e.g. design notes, stale code, scratch markup) without provoking parse errors.
