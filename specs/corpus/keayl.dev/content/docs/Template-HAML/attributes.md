---
title: Attributes
slug: attributes
order: 4
description: Template::HAML supports a Ruby-style attribute hash on any tag:
toc: true
---
Template::HAML supports a Ruby-style attribute hash on any tag:

```haml
%a{href: '/about', title: 'About us'} About
```

```html
<a href='/about' title='About us'>About</a>
```

## Value types

Today the supported value types inside `{ ... }` are:

- Single-quoted strings: `'value'`
- Double-quoted strings: `"value"` (supports `#{...}` interpolation)
- Numbers: `123`, `-4.5`
- Booleans: `True`, `False`, `Nil`
- Symbols (treated as strings): `:value`
- Arrays of the above: `['a', 'b']`
- Hashes of the above: `{ id: 1, role: 'main' }`

```haml
%input{type: :text, name: 'email'}
%a{href: "/", class: 'home'} Home
```

To embed a Raku local variable, use double-quoted interpolation or an
attribute splat — never a bareword `$var`. A hash whose value the parser
cannot read raises `X::HAML::ParseFail` pointing at the opening `{`:

```haml
%a{href: "#{$url}"} go            -- works (interpolation)
%a{|$attrs} go                     -- works (splat)
%a{href: $url} go                  -- X::HAML::ParseFail
%a{aria-label: 'home'} hi          -- X::HAML::ParseFail (hyphen bareword key)
```

Hyphenated keys must be quoted:

```haml
%a{'aria-label' => 'home'} hi
%a{"data-id" => 1} click
```

The same rule applies to HTML-style `(...)` attrs: values must be quoted
strings; a `$var` value or any other unparseable construct raises
`X::HAML::ParseFail`.

## Multiple attributes

Separate attributes with commas:

```haml
%a{href: '/', class: 'home', id: 'home-link'} Home
```

```html
<a href='/' class='home' id='home-link'>Home</a>
```

## Class shorthand merges with `class:`

The `.classname` shorthand on a tag merges with any `class:` entry in the attribute hash. The two forms compose:

```haml
%p.lead{class: 'intro'} Welcome
```

```html
<p class='lead intro'>Welcome</p>
```

## Multi-line attribute hashes

An attribute hash, an HTML-style attribute list, an array value, or a nested hash may span multiple lines. The tag does not close until the matching `}` (or `)`) is seen.

```haml
%a{
  href: '/',
  title: 'home'
}
```

```html
<a href='/' title='home'></a>
```

Nested hashes and arrays may also span lines:

```haml
%a{
  class: ['foo', 'bar'],
  data: {
    id: 1,
    role: 'main'
  }
}
```

```html
<a class='foo bar' data-id='1' data-role='main'></a>
```

HTML-style attributes may also be split across lines:

```haml
%a(
  href='/'
  title='home'
)
```

```html
<a href='/' title='home'></a>
```

Content and child indentation pick up after the closing brace:

```haml
%ul{
  class: 'list'
}
  %li one
  %li two
```

```html
<ul class='list'>
  <li>one</li>
  <li>two</li>
</ul>
```

Source line numbers are preserved across multi-line hashes, so children render and report at their actual line in the source.

## Attribute splat (`|$expr`)

Prefix any expression inside `{ ... }` with `|` to merge its result into the
tag's attributes:

```raku
HAML.render(
  :src('%a{|$attrs} link' ~ "\n"),
  :locals(%(attrs => %( :href('/'), :title('home') ))),
);
```

```html
<a href='/' title='home'>link</a>
```

The splat expression is plain Raku. Locals are bound with the `$` sigil, so
write `|$attrs`, `|$obj.attrs`, `|$mk()`, and so on. The expression must
evaluate to one of:

- a `Hash` (keys are emitted in alphabetical order),
- a single `Pair`,
- a list of `Pair`s (emitted in declaration order).

### Composes with literal pairs

Splats interleave freely with literal `key: value` entries. Within a single
tag, later entries override earlier ones for the same key:

```haml
%a{href: '/from-literal', |$attrs} go
```

If `$attrs<href>` is `/from-splat`, the rendered `href` is `/from-splat`.
Reverse the order to make the literal win:

```haml
%a{|$attrs, href: '/wins'} go
```

### Multiple splats

`%tag{|$a, |$b}` is valid. Later splats override earlier ones for the same
non-`class`/non-`id` key.

### `class:` and `id:` accumulate

Splatted `class:` values merge with shorthand classes and any literal `class:`
entry — duplicates are removed and the result is space-joined. Splatted `id:`
values concatenate with shorthand ids and literal `id:` entries using `_`.

```haml
%div.a{class: 'b', |$h} hi
```

With `$h<class>` set to `'c'`, the rendered class is `'b c a'` (literal and
splat in declaration order, shorthand classes appended).

```haml
%div#one{id: 'two', |$h}
```

With `$h<id>` set to `'three'`, the rendered id is `'one_two_three'`
(shorthand ids first, then literal and splat values in declaration order).

### Nested hashes work too

Splatting a hash whose values include another hash under `data:` or `aria:`
expands into the same hyphenated attributes as a literal nested hash:

```raku
HAML.render(
  :src('%a{|$h}' ~ "\n"),
  :locals(%(h => %( :data({ :id(1), :role('main') }) ))),
);
```

```html
<a data-id='1' data-role='main'></a>
```

## `data:` / `aria:` hyphenation

By default, keys under `data:` and `aria:` hashes are emitted verbatim:

```haml
%a{data: {fooBar: 1}}
```

```html
<a data-fooBar='1'></a>
```

Enable the [`hyphenate-data-attrs`](/docs/Template-HAML/config) config option to convert
`camelCase` keys to `kebab-case`. The conversion is recursive — every level
of a nested hash is rewritten:

```raku
my $cfg = Template::HAML::Config.new(:hyphenate-data-attrs);
HAML.render(:src('%a{data: {fooBar: 1}}' ~ "\n"), :config($cfg));
```

```html
<a data-foo-bar='1'></a>
```
