---
title: Whitespace
slug: whitespace
order: 9
description: Template::HAML supports HAML's whitespace-removal modifiers and whitespace-preservation behaviors so you can control how generated HTML is laid out.
toc: true
---
Template::HAML supports HAML's whitespace-removal modifiers and whitespace-preservation behaviors so you can control how generated HTML is laid out.

## Outer trim `>`

A trailing `>` on a tag strips whitespace immediately around the tag, both before its open tag and after its close tag.

```haml
%p first
%p> middle
%p last
```

```html
<p>first</p><p>middle</p><p>last</p>
```

This works for any tag, including void elements:

```haml
%img
%img>
%img
```

```html
<img><img><img>
```

## Inner trim `<`

A trailing `<` on a tag strips whitespace immediately *inside* the tag, both after the open tag and before the close tag.

```haml
%blockquote<
  %div
    Foo!
```

```html
<blockquote><div>
  Foo!
</div></blockquote>
```

When combined with content on the same line plus a single child, the child renders inline:

```haml
%p< hello
  %strong world
```

```html
<p>hello<strong>world</strong></p>
```

## Combined `<>` / `><`

Both modifiers can be combined in either order:

```haml
%p<>
  %a hi
```

```html
<p><a>hi</a></p>
```

## Preserved tags

The `pre` and `textarea` elements automatically preserve their inner whitespace by replacing newlines with the `&#x000A;` HTML entity. This keeps the rendered display intact while still allowing HAML to indent the source.

```haml
%pre
  Line 1
  Line 2
```

```html
<pre>&#x000A;  Line 1&#x000A;  Line 2&#x000A;</pre>
```

The list of preserved elements is configurable via the `preserve` option on `Template::HAML::Config`:

```raku
my $cfg = Template::HAML::Config.new(:preserve('pre', 'textarea', 'code'));
HAML.render(:src($haml), :config($cfg));
```

## Global whitespace removal

To apply `>` and `<` to every tag at once, set `remove-whitespace: True` on the
config. See [Configuration](/docs/Template-HAML/config#remove-whitespace) for details. Preserved
tags keep their inner whitespace but still have outer whitespace stripped.

## Forced preserve `~`

The `~` operator works like `=` (eval and emit), but additionally replaces newlines in the result with `&#x000A;`. This is useful when an interpolated string contains newlines you want to keep literal in the rendered output.

```haml
~ "line1\nline2"
```

```html
line1&#x000A;line2
```

Like `=`, the result is HTML-escaped by default. Disable escaping globally with `escape_html => False` on the config.
