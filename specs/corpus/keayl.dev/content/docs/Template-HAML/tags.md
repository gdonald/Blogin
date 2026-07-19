---
title: Tags
slug: tags
order: 3
description: A tag in HAML is introduced by a sigil at the start of a line:
toc: true
---
A tag in HAML is introduced by a sigil at the start of a line:

| Sigil | Meaning                              |
|-------|--------------------------------------|
| `%`   | Element name (`%p`, `%div`, `%h1`)   |
| `.`   | Class on a `<div>` (`.container`)    |
| `#`   | Id on a `<div>` (`#main`)            |

## Element names

```haml
%p Hello
%div A block
%h1 Title
```

renders to:

```html
<p>Hello</p>
<div>A block</div>
<h1>Title</h1>
```

## Class shorthand

`%tag.classname` adds a class to an element:

```haml
%p.lead Welcome
```

```html
<p class='lead'>Welcome</p>
```

You can chain multiple classes:

```haml
%p.lead.intro Welcome
```

```html
<p class='lead intro'>Welcome</p>
```

## Id shorthand

`%tag#name` sets the element's id:

```haml
%section#main
```

```html
<section id='main'></section>
```

## Combining shorthand and attributes

The class shorthand merges with a `class:` attribute hash entry:

```haml
%p.lead{class: 'intro'} Welcome
```

```html
<p class='lead intro'>Welcome</p>
```

See [Attributes](/docs/Template-HAML/attributes) for the full attribute hash syntax.

## Shorthand interpolation

`#{ ... }` and `!{ ... }` are recognized inside class and id shorthand and
may be combined with literal segments (including hyphens):

```haml
%div.item-#{ $id }
%div#row-#{ $n }
%li.item-#{ $id }#row-#{ $n }
```

with `:locals(%(id => 7, n => 4))` renders to:

```html
<div class='item-7'></div>
<div id='row-4'></div>
<li id='row-4' class='item-7'></li>
```

Multiple interpolations may appear in a single shorthand segment:

```haml
%div.a-#{ $x }-b-#{ $y }
```

with `:locals(%(x => 1, y => 2))` renders to:

```html
<div class='a-1-b-2'></div>
```

`#{ ... }` HTML-escapes its value; `!{ ... }` emits it raw:

```haml
%div.item-#{ $s }
```

with `:locals(%(s => 'a&b'))` renders to:

```html
<div class='item-a&amp;b'></div>
```

Interpolated shorthand merges with hash-style `class:` and `id:` entries
using the same rules as plain shorthand — classes accumulate, ids
concatenate with `_`:

```haml
%p.item-#{ $id }{class: 'active'}
%p#row-#{ $n }{id: 'extra'}
```

with `:locals(%(id => 9, n => 5))` renders to:

```html
<p class='active item-9'></p>
<p id='row-5_extra'></p>
```

To produce a literal `#{...}` in a shorthand segment, escape the `#` with
a backslash:

```haml
%div.literal-\#{x}
```

renders to:

```html
<div class='literal-#{x}'></div>
```

## Object reference

A bracketed expression after the tag name (and any shorthand) derives a
`class` and an `id` from a Raku object at render time:

```haml
%div[$user]
```

with `$user` an instance of class `User` whose `.id` returns `42`,
renders to:

```html
<div id='user_42' class='user'></div>
```

The class is the object's short type name converted from `CamelCase` to
`snake_case` (e.g. `BlogPost` → `blog_post`). The id is `<class>_<.id>`.
If the object has no `.id` method, only the class is emitted.

To customize the derived class name, define a `haml-object-ref` method
on the class:

```raku
class Page {
  has Int $.id;
  method haml-object-ref { 'cms-page' }
}
```

```haml
%div[$page]
```

```html
<div id='cms-page_7' class='cms-page'></div>
```

### Prefix form

A second argument is treated as a prefix prepended to both the class and
the id with an underscore:

```haml
%div[$user, 'greeting']
```

```html
<div id='greeting_user_42' class='greeting_user'></div>
```

The prefix may be any Raku expression — a string literal, a local
variable, or a method call. A `Pair` value (e.g. `:greeting`) uses its
key for parity with Ruby's symbol form.

### Combining with shorthand and attribute hashes

Object reference fits between the shorthand and the attribute hashes:

```haml
%p.lead#hero[$user]{class: 'active'}
```

The object-ref class is prepended to the existing classes, and the
object-ref id is prepended to any other id (joined with `_`):

```html
<p id='user_42_hero' class='user lead active'></p>
```

An undefined object emits neither class nor id:

```haml
%div[$missing]
```

```html
<div></div>
```
