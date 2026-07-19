---
title: Canonical form
slug: canonical-form
order: 20
description: haml fmt rewrites a .haml source file into its canonical form: the single representative chosen from every set of HAML sources that parse to the same AST....
toc: true
---
`haml fmt` rewrites a `.haml` source file into its **canonical form**: the
single representative chosen from every set of HAML sources that parse to
the same AST. This page declares the rules. The formatter aims to make
`fmt(fmt(x)) == fmt(x)` (idempotent) and `parse(fmt(x)) == parse(x)`
(semantics-preserving) for every input the parser accepts.

If a rule below is silent on some construct, the formatter leaves the
existing source unchanged for that construct.

## File-level rules

- Encoding is UTF-8, no BOM.
- Line endings are LF (`\n`). CR and CRLF are normalized to LF.
- The file ends with exactly one trailing newline.
- No leading blank lines.
- No trailing whitespace on any line.
- At most one consecutive blank line between siblings; runs of blank
  lines collapse to a single blank line.

## Indentation

- Two spaces per level. Tabs and odd-width indents are rejected by the
  parser (`X::IllegalIndent`); the formatter never produces them.
- A child line is indented exactly two more spaces than its parent.
- Continuation lines inside a multi-line attribute hash, HTML-style
  attribute list, or multi-line expression are indented two spaces
  beyond the line that opens the construct.

## Tag head

The canonical order of a tag head, left to right:

```
%name  .class…  #id  [obj-ref]  {hash-attrs}  (html-attrs)  trim  self-close  content
```

- `%name` is omitted when at least one shorthand (`.class` or `#id`) is
  present *and* the implicit element is `div`. `%div.foo` canonicalizes
  to `.foo`; `%span.foo` is preserved.
- Shorthand order is *all* classes first (in source order), then the id.
  Mixed shorthand like `%li#row.item.lead` is reordered to
  `%li.item.lead#row`. At most one `#id` shorthand is allowed by the
  parser; the formatter never invents a second.
- The object-reference bracket `[expr]` (and the optional prefix
  `[expr, 'prefix']`) sits between shorthand and any attribute hash.
- Hash-style `{ … }` precedes HTML-style `( … )` when both are present.
- Whitespace-trim modifiers `<`, `>`, `<>` appear immediately before the
  self-close slash; the canonical spelling is `<>` (not `><`).
- The self-close `/` appears last in the head. Void elements (`img`,
  `br`, `hr`, `input`, `meta`, `link`, `area`, `base`, `col`, `embed`,
  `param`, `source`, `track`, `wbr`) never carry an explicit `/`.

## Attributes

### Form

- Hash form `{ key: value, … }` is canonical. HTML-style `( … )`
  attribute lists are rewritten to hash form: once parsed, the two
  forms are semantically identical for the values HTML-style admits
  (string-typed only), and the parser does not preserve the source
  spelling.
- Inside `{ … }`, keys use the bareword form (`key: value`) when the
  key is a valid Raku identifier (matches `<[A..Za..z_]> \w*`). Keys
  that require quoting (containing `-`, starting with a digit, etc.)
  use the `'key' => value` rocket form.

### Spacing

- One space after the opening `{`, one space before the closing `}`
  on single-line hashes: `{ href: '/', title: 'home' }`.
- No space between key and colon: `href:` not `href :`.
- One space after every comma; no space before a comma.

### Layout

- A single-line tag head plus content fits on one source line if the
  result is at most **100 columns**. Past that, the hash spills:

  ```haml
  %a{
    href: '/about',
    title: 'About us',
  } About
  ```

- In multi-line form: each entry on its own line, indented two spaces
  beyond the opening line, with a trailing comma after every entry
  including the last.

### Order

Entry order is **preserved**, never reordered. Order is observable:

- `class:` and `id:` accumulate with shorthand and earlier splats.
- Plain keys collide last-wins; splat vs. literal order changes which
  value wins.

The formatter must not move splats (`|$expr`) past literal entries or
each other.

### Quoting

- String values use single quotes by default: `title: 'home'`.
- Switch to double quotes when the value contains a single quote, a
  newline/tab/CR escape, or uses `#{…}` / `!{…}` interpolation.
- Symbol form (`:value`) is normalized to a quoted string. The
  attribute renderer cannot distinguish the two after parsing, so
  there is no canonical spelling to preserve.

### Splats

- `|$expr` keeps its position relative to surrounding entries.
- No space between `|` and the expression.

## Embedded code

- One space follows the operator: `= expr`, `- stmt`, `!= raw`,
  `&= forced`, `~ preserve`.
- Multi-line expressions (trailing comma or unbalanced brackets) keep
  continuation lines aligned to the column of the first non-operator
  character on the opening line.

## Plain text

- A line that is purely literal text and does not begin with a HAML
  sigil (`%`, `.`, `#`, `=`, `-`, `!`, `&`, `~`, `/`, `:`, `\`, `!!!`)
  is emitted verbatim.
- A leading `\` escape is kept only when needed to disambiguate the
  first character. The formatter strips a redundant `\` (one whose
  removal still parses as plain text).

## Comments

- HTML comment lead: `/ text` with exactly one space after the slash
  on the single-line form; the block form is `/` on its own line with
  the body indented.
- Conditional comment: `/[expr]`. The bracketed expression has no
  surrounding whitespace.
- Revealed conditional: `/![expr]`.
- Silent comment: `-# text` with one space after `-#` for the inline
  form. Block silent comments place `-#` alone on a line with the
  opaque body indented.

## Doctype

- The doctype line (if present) is the first non-blank line of the
  file.
- Capitalization of named variants is preserved (`!!! Strict`,
  `!!! 1.1`).
- Exactly one space between `!!!` and any argument.

## Filters

- Filter head is `:name` flush at the parent indent.
- Body is indented two spaces beyond the filter head.
- The smallest indent across the body is **not** further normalized —
  relative indentation inside the body is part of the filter's
  contract.
- A blank line is preserved between the filter and its next sibling
  when present in source; the formatter does not insert one.

## What is *not* canonicalized

The formatter preserves all of the following exactly as the user wrote
them, because rewriting would either change semantics or destroy
information the parser cannot recover:

- Splat order relative to literal entries.
- The text of any filter body (filters are opaque to the formatter).
- The text of `-#` silent-comment bodies.
- Conditional-comment expressions inside `[ … ]`.
- Whitespace inside double-quoted strings and `#{…}` interpolations.

These constructs *are* canonicalized — the parser collapses them to a
single representation, so the formatter cannot recover the original
spelling:

- HTML-style attribute lists `( … )` collapse to hash-style `{ … }`.
- Symbol attribute values (`:value`) collapse to quoted strings.

## Idempotency

For every source `x` that parses successfully, `fmt(x)` parses to the
same AST as `x`, and `fmt(fmt(x)) == fmt(x)` byte-for-byte. The
emitter sub-tasks (`haml fmt` for tags, attributes, filters, etc.)
each carry their own idempotency tests, and the CLI integration
includes an end-to-end check that the full file round-trips.
