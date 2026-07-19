---
title: Render context
slug: context
order: 15
description: HAML.render accepts a :context named argument — a Raku object whose methods become reachable inside templates as bare identifiers in expression position....
toc: true
---
`HAML.render` accepts a `:context` named argument — a Raku object whose methods
become reachable inside templates as **bare identifiers** in expression
position. This is the template's "view context": a place to put template-facing
helpers without polluting global scope.

```raku
class MyView {
  has Str $.title;
  method greeting { 'world' }
}

HAML.render(
  :src(q:to/HAML/),
    %h1= title
    %p= greeting
    HAML
  :context(MyView.new(:title('Hello'))),
);
```

renders:

```html
<h1>Hello</h1>
<p>world</p>
```

## Bare identifiers

A "bare identifier" is an expression that is exactly one name — letters,
digits, underscore, hyphen — with no parentheses, sigils, or operators.
Examples:

| Expression   | Bare identifier? |
| ------------ | ---------------- |
| `title`      | yes              |
| `user-count` | yes              |
| `$title`     | no — has sigil   |
| `title()`    | no — has parens  |
| `title.uc`   | no — has `.`     |

A bare identifier in `=`, `!=`, `&=`, `~`, or in a control-flow condition
(`if`, `unless`, `elsif`, `while`, `repeat`, `given`) resolves through:

1. **Locals** — if `%locals{name}` exists, use it.
2. **Context method** — otherwise if `context.^can(name)` is true, call
   `context.name`.
3. **Fallback to eval** — otherwise the expression is handed to the normal
   Raku evaluator. If the name isn't a valid Raku identifier in scope, you'll
   get `X::HAML::Eval`.

## Context methods with arguments

A context method is also callable as a bare name **with arguments**, so a
helper that takes parameters reads like a plain function call, no sigil:

```raku
class AppView does Template::HAML::HelpersRole {
  method link-to($text, $href) { qq{<a href="$href">$text</a>} }
}
```

```haml
%nav
  != link-to('Home', '/')
  != link-to(label, url)
```

Each context method named in an expression is bound as a lexical routine for
that render, dispatching to the context. Locals are still referenced with their
sigil (`$label`), since only standalone bare identifiers resolve to a local.
Built-in helpers (`surround`, `list-of`, …) remain callable as before. This
works in the interpreted renderer and the default direct-emit renderer.

```haml
- if is-admin
  %p= admin-greeting
- else
  %p= public-greeting
```

## Overriding a built-in helper

A context method takes precedence over the built-in helper of the same name. To
replace `surround`, `partial`, `list-of`, or any other built-in for a given
render, define a method with that name on your context:

```raku
class AppView does Template::HAML::HelpersRole {
  method surround($pre, $post, &block) { "{$pre}OVERRIDDEN{$post}" }
}
```

```haml
%div
  != surround('[', ']', { 'inner' })
```

renders `<div>[OVERRIDDEN]</div>` instead of the built-in `[inner]`. Because the
default `ViewContext` exposes the built-ins as methods, every built-in name is
resolved through the context, so overriding one is just defining it.

## Declaring helpers explicitly

Helper discovery normally introspects the context with `.^methods`. Helpers
installed in a way that introspection does not report, such as methods added at
runtime with `^add_method` or handled through `FALLBACK`, are invisible to that
scan. Declare them with a `haml-helper-names` method that returns their names:

```raku
class DynamicView does Template::HAML::HelpersRole {
  method haml-helper-names { <ping pong> }
  method FALLBACK($name, |args) { ... }
}
```

When `haml-helper-names` is present its list is trusted verbatim: each entry is
kept if it is a valid helper name (starts with a lowercase letter or underscore,
then word characters or hyphens), then de-duplicated and sorted. The list is not
cached, so a context whose helper set varies per instance can return a different
list each time.

## Helpers and the compiled cache

A context's helper names take part in the compiled-template cache key used by
`render-cached` and `render-file-cached`. The compiled module is specialized to
the helper set, so the same source rendered against two contexts with different
helpers compiles and caches separately, and a helper is never resolved against a
module compiled for a context that lacked it. See
[On-disk compiled-template cache](/docs/Template-HAML/api#on-disk-compiled-template-cache).

## Default context

If you don't pass `:context`, the renderer constructs a
`Template::HAML::ViewContext` for you. It composes the
`Template::HAML::HelpersRole` role, so every built-in helper (`surround`,
`escape-once`, `list-of`, `find-and-preserve`, `html-safe`, `capture-haml`,
`yield`, `content-for`, `tab-up`, `tab-down`, `haml-concat`) is reachable as
a method on the context.

## Extending the context

Two composition points are supported.

**Mix in the helper role into your own class:**

```raku
use Template::HAML::HelpersRole;

class AppContext does Template::HAML::HelpersRole {
  has $.user;
  method current-user-name { $!user.name }
}

HAML.render(:src($src), :context(AppContext.new(:$user)));
```

The role brings the built-in helpers along, so you can add your own methods
without losing `surround`, `list-of`, etc.

**Subclass `ViewContext`:**

```raku
use Template::HAML::ViewContext;

class AppContext is Template::HAML::ViewContext {
  method site-name { 'MySite' }
}
```

Either approach works; the role form is slightly more flexible because you
can compose other roles alongside it:

```raku
role Greeter { method hello   { 'hi'     } }
role Footer  { method footer  { 'fineprint' } }

class AppContext does Template::HAML::HelpersRole does Greeter does Footer { }
```

Bare identifiers (`hello`, `footer`, `surround`, …) resolve against every
composed role.

## Locals still win

When both a local and a context method have the same name, the local wins:

```raku
class MyView {
  method title { 'from-context' }
}

HAML.render(
  :src("%h1= title\n"),
  :context(MyView.new),
  :locals(%(title => 'from-locals')),
);
# → <h1>from-locals</h1>
```

This precedence keeps per-render values from being silently shadowed by a stray
helper method.
