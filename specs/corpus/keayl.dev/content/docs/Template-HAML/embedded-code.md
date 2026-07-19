---
title: Embedded code
slug: embedded-code
order: 10
description: Template::HAML lets you embed Raku expressions directly in a template. Each embedded line begins with an output operator and is followed by a Raku...
toc: true
---
Template::HAML lets you embed Raku expressions directly in a template. Each
embedded line begins with an output operator and is followed by a Raku
expression.

## Operators

| Operator | Behavior                                                                                                    |
| -------- | ----------------------------------------------------------------------------------------------------------- |
| `=`      | Evaluate the expression, HTML-escape the result, emit it.                                                   |
| `!=`     | Evaluate the expression, emit the result without escaping.                                                  |
| `&=`     | Evaluate the expression, force HTML-escape, emit it.                                                        |
| `==`     | Emit the line verbatim, interpolating `#{...}` / `!{...}` only — the body is *not* evaluated as Raku.       |
| `-`      | Evaluate the expression for side effects only — emit nothing.                                               |
| `~`      | Like `=`, but additionally replace newlines in the result with `&#x000A;`. See [Whitespace](/docs/Template-HAML/whitespace). |

```haml
= 1 + 2
- my $x = 'hello'
= $x
!= '<b>raw</b>'
```

renders to:

```html
3
hello
<b>raw</b>
```

## Locals

Pass values into a template with the `:locals` named argument. Each key is bound
as a `$key` lexical visible to embedded expressions.

```raku
HAML.render(
  :src(q:to/HAML/),
    %h1= $title
    %p Visitor count: #{ $count }
    HAML
  :locals(%(:title<Welcome>, :count(42))),
);
```

Locals of any type are bound under the `$` sigil; reach into structured values
with the usual postfix syntax (`$items.elems`, `$cfg<title>`, `$user.name`).

Locals are also visible inside `#{...}` and `!{...}` interpolations in tag
content, plain text, and double-quoted attribute strings — see [Plain
text](/docs/Template-HAML/plain-text) for the full interpolation syntax.

## Escaping

`=` escapes `& < > " '` by default. Use `!=` only when the value is already
known-safe HTML.

## Multi-line expressions

An embedded code line continues on the next line in two situations:

1. **Trailing comma.** A line whose last non-whitespace character is `,`
   joins the next line with a single space.
2. **Unbalanced brackets.** A line that opens more `(`, `[`, or `{` than it
   closes joins the next line; continuation stops once the brackets balance
   *and* the line no longer ends with a comma.

Both rules ignore characters that appear inside single- or double-quoted
strings.

```haml
= [1,
   2,
   3].sum

- my %config = (
    title => 'Welcome',
    count => 42,
  )

= sprintf('%s: %d',
          $title,
          $count)
```

If the source ends while an expression is still unbalanced (open brackets or a
trailing comma), the parse raises
[`X::HAML::UnbalancedExpression`](/docs/Template-HAML/api#exceptions) pointing at the line
where the expression started.

Filter bodies (`:plain`, `:javascript`, …) and silent-comment bodies (`-#`)
are opaque text — lines that look like statements inside them are *not*
joined.

## Helpers

A handful of helper subroutines (`html-safe`, `escape-once`, `surround`,
`precede`, `succeed`, `list-of`, `find-and-preserve`, `capture-haml`) are
preloaded into the eval scope. See [Helpers](/docs/Template-HAML/helpers).

## Bare identifiers and the render context

An expression that is just a single bare identifier (no sigil, no parens) is
resolved against locals first, then against methods on the
[render context](/docs/Template-HAML/context). See [Render context](/docs/Template-HAML/context) for the full
rules and how to plug in your own helper methods.

## Errors

Compilation or runtime failures inside embedded code are wrapped in
[`X::HAML::Eval`](/docs/Template-HAML/api#exceptions), which carries the offending source
line and column for diagnostics.
