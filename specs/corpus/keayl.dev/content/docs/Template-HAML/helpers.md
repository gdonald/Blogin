---
title: Helpers
slug: helpers
order: 14
description: Template::HAML::Helpers exports a small set of convenience subroutines that embedded Raku code can call directly. They are visible to every =, !=, &=, and...
toc: true
---
`Template::HAML::Helpers` exports a small set of convenience subroutines that
embedded Raku code can call directly. They are visible to every `=`, `!=`,
`&=`, and `-` expression without explicit `use`.

```raku
use Template::HAML::Helpers;
```

The module is loaded automatically by the renderer. Importing it from your own
code is only necessary when you want to call the helpers from outside a
template (e.g. in tests).

## `html-safe($s)`

Wraps a string in a `SafeString` sentinel so the renderer leaves it
unescaped, even when [`escape_html`](/docs/Template-HAML/config) is on.

```haml
= html-safe('<b>raw</b>')
```

renders:

```html
<b>raw</b>
```

`!=` already disables escaping per-line. Reach for `html-safe` when you want
the value to flow through other helpers (e.g. `surround`) and remain unescaped
at the final `=` site.

## `escape-once($s)`

HTML-escapes `&`, `<`, `>`, `"`, `'` — but skips ampersands that are already
the start of a numeric (`&#39;`, `&#x27;`) or named (`&amp;`) entity.

```raku
escape-once('&amp; <');   # → '&amp; &lt;'
escape-once('a & b');     # → 'a &amp; b'
```

Useful when round-tripping content that has already been partially escaped.

## `surround($pre, $post, &block)` / `precede($pre, &block)` / `succeed($post, &block)`

Concatenate fixed text around the trimmed result of a block.

```haml
%p
  != surround('(', ')', { 'middle' })
%p
  != precede('* ', { 'item' })
%p
  != succeed('.',  { 'sentence' })
```

renders:

```html
<p>
  (middle)
</p>
<p>
  * item
</p>
<p>
  sentence.
</p>
```

The block's return value is stringified and stripped of leading/trailing
whitespace before the fixed text is added.

## `list-of($items, &block)`

Wraps each call to the block in a `<li>` element and joins the results with
newlines.

```haml
%ul
  != list-of($items, -> $x { $x.uc })
```

with locals `:items(<a b c>)` renders:

```html
<ul>
  <li>A</li>
<li>B</li>
<li>C</li>
</ul>
```

`$items` accepts any `.list`-able value.

## `find-and-preserve($html)`

Scans a rendered HTML fragment for `<pre>` and `<textarea>` blocks and replaces
literal newlines inside them with `&#x000A;`. Lets you render with `:pretty`
output style without disturbing whitespace-sensitive content nested deeper than
the [`preserve`](/docs/Template-HAML/config) tag list reaches.

```raku
find-and-preserve("<pre>line1\nline2</pre>");
# → '<pre>line1&#x000A;line2</pre>'
```

## `haml-concat($s)`

Appends a string to the current rendering buffer. When called inside a `=` or
`-` expression, the value is emitted at the current indent — without
HTML-escaping, since `haml-concat` returns a `SafeString`. The most useful
shape is calling `haml-concat` from inside helpers that need to write more
than one chunk:

```haml
- haml-concat('one'); haml-concat('two')
```

renders:

```html
one
two
```

When called outside a render context, `haml-concat` just returns a
`SafeString` wrapping its argument.

## `tab-up($n = 1)` / `tab-down($n = 1)`

Adjust the current output indent for everything that follows. Useful when a
helper emits a block of nested markup and wants the surrounding template's
indent to line up.

```haml
%div
  - tab-up
  %p deep
  - tab-down
  %p back
```

renders:

```html
<div>
    <p>deep</p>
  <p>back</p>
</div>
```

Adjustments are sticky — `tab-up` keeps applying to siblings until a matching
`tab-down` undoes it. The offset resets at the start of every `HAML.render`
call.

## `capture-haml(&block)`

Calls the block, treats its return value as HAML source, and renders it to a
string.

```haml
- my $fragment = capture-haml({ '%p hi' });
= html-safe($fragment)
```

The block must return a HAML source string. There is no implicit "HAML block"
syntax — Ruby HAML's `capture_haml do ... end` form is not available because
embedded code in this implementation is plain Raku, not HAML.

## `page-class(:$controller, :$action, :$separator = '-', :$prefix = '')`

Emits a default class derived from a controller/action pair. Useful as the body
class on a layout so per-page styles can target `.controller-action` selectors.

With no arguments, `page-class` looks up `controller-name` / `action-name` on
the current [render context](/docs/Template-HAML/context) (falling back to `controller` /
`action`). Explicit keyword arguments override the context values. Each value
is lowercased and non-word characters become single dashes.

```haml
!!! 5
%html
  %body.#{page-class()}
    = yield
```

Given a context with `controller-name → 'users'` and `action-name → 'show'`,
the body opens as:

```html
<body class="users-show">
```

Overrides and options:

```raku
page-class(:controller<users>, :action<show>);          # 'users-show'
page-class(:controller<users>, :action<show>, :separator<_>); # 'users_show'
page-class(:controller<users>, :action<show>, :prefix<page>); # 'page-users-show'
page-class(:controller('Admin::Users'), :action('New Post')); # 'admin-users-new-post'
```

When neither argument is supplied and the context has no controller/action
hooks, `page-class` returns the empty string.
