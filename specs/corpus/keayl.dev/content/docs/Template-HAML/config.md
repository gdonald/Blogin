---
title: Configuration
slug: config
order: 16
description: Rendering options live on a Template::HAML::Config object. Pass one to HAML.render(:config(...)) for per-render overrides, or to HAML.new(:config(...))...
toc: true
---
Rendering options live on a `Template::HAML::Config` object. Pass one to
`HAML.render(:config(...))` for per-render overrides, or to
`HAML.new(:config(...))` for an instance default.

```raku
use Template::HAML;
use Template::HAML::Config;

my $cfg = Template::HAML::Config.new(:format<xhtml>);

HAML.render(:src("%br\n"), :config($cfg));   # "<br />\n"

my $h = HAML.new(:config($cfg));
$h.render(:src("%br\n"));                     # uses the instance config
```

## Options

| Option                | Default      | Effect                                                                                          |
|-----------------------|--------------|-------------------------------------------------------------------------------------------------|
| `format`              | `'html5'`    | One of `html5`, `html4`, `xhtml`. Drives default doctype, void self-close, boolean attrs.       |
| `escape-html`         | `True`       | When false, `=` does not HTML-escape its output. `&=` always escapes; `!=` never escapes.       |
| `escape-attrs`        | `True`       | When false, attribute values are emitted raw (only the active quote char is escaped).           |
| `output-style`        | `'pretty'`   | `pretty` (indented) or `ugly` (single-line, no inter-tag whitespace).                            |
| `attr-quote`          | `"'"`        | Quote character used around attribute values; `"'"` or `'"'`.                                    |
| `encoding`            | `'utf-8'`    | Used for `!!! XML` output and to decode `Blob`/file template sources. See [Encoding](#encoding). |
| `suppress-eval`       | `False`      | When true, `=`, `-`, `!=`, `&=` produce no output and the expression body is not evaluated.     |
| `cdata`               | `False`      | When true, the `:javascript` and `:css` filters wrap their body in a `CDATA` marker even outside XHTML. |
| `mime-type`           | `''`         | When set, `:javascript` / `:css` emit a `type="…"` attribute with this value.                    |
| `hyphenate-data-attrs`| `False`      | When true, `camelCase` keys under `data:` / `aria:` hashes are emitted as `kebab-case`.          |
| `output-indent-width` | `2`          | Number of spaces per indent level in pretty output.                                              |
| `remove-whitespace`   | `False`      | When true, every tag is treated as if both `>` and `<` modifiers were present (except preserved tags keep their inner whitespace). |
| `trace`               | `False`      | When true, render-time `X::HAML::Eval` errors include the compiled-block source and a template-line → block-line breadcrumb. See [trace](#trace). |
| `emit`                | `'direct'`   | Codegen path: `'direct'` inlines `$out ~= ...` Raku, `'ast'` rebuilds the parse tree at render time. Overridden by `HAML_DEFAULT_EMIT=ast`. See [API: Direct-emit codegen](/docs/Template-HAML/api#direct-emit-codegen-default). |
| `autoclose`           | HTML5 voids  | List of element names that auto-self-close: `area base br col embed hr img input link meta param source track wbr`. |
| `preserve`            | `<pre textarea>` | List of elements whose inner whitespace is preserved.                                       |

## Format effects

* **html5**: void elements emit `<br>`; boolean attributes are bare (`<input disabled>`); default doctype is `<!DOCTYPE html>`.
* **html4**: same shape as HTML5 but the default doctype is HTML 4.01 Transitional.
* **xhtml**: void elements emit `<br />` (note the space-slash); boolean attributes use the `name="name"` form (`<input disabled="disabled" />`); default doctype is XHTML 1.0 Transitional.

## Escape behavior

| Operator | Default (`escape-html: True`) | `escape-html: False` |
|----------|-------------------------------|----------------------|
| `=`      | escape                        | no escape            |
| `&=`     | escape (force)                | escape (force)       |
| `!=`     | no escape                     | no escape            |
| `==`     | interpolation only            | interpolation only   |

`escape-attrs` is independent of `escape-html` and only affects attribute values.

## Output style

`output-style: 'ugly'` emits all output on a single line with leading whitespace
trimmed and blank lines dropped. Useful for production where the extra bytes
matter:

```haml
%div
  %p hi
```

Pretty (default):

```html
<div>
  <p>hi</p>
</div>
```

Ugly:

```html
<div><p>hi</p></div>
```

## remove-whitespace

`remove-whitespace: True` strips whitespace immediately inside and around every
tag, as if both `>` and `<` whitespace modifiers were applied to every element.
Tags listed in `preserve` (default: `pre`, `textarea`) retain their inner
whitespace but still have outer whitespace stripped.

```haml
%div
  %p hi
  %pre
    line 1
    line 2
```

Renders as:

```html
<div><p>hi</p><pre>&#x000A;  line 1&#x000A;  line 2&#x000A;</pre></div>
```

Unlike `output-style: 'ugly'`, plain-text lines keep their own newlines, so
text inside tags reads as written. The two options are independent and may be
combined.

## Autoclose

The `autoclose` option replaces the built-in void list. To add a custom void
element while keeping the defaults, pass the full list:

```raku
my $cfg = Template::HAML::Config.new(
  :autoclose(<area base br col embed hr img input link meta param source track wbr custom-tag>),
);
```

## trace

Embedded Raku is evaluated by wrapping each template expression in a closure
and `EVAL`-ing it. When that closure fails — at compile time (bad Raku) or at
run time (e.g. an undefined sub) — `X::HAML::Eval` is thrown carrying the
template line and column.

`trace: True` augments those errors with two extra fields:

* `block-source` — the exact wrapped-closure source that was handed to `EVAL`.
* `block-line` — the line within that source where the user expression lives
  (`2`; line 1 is the `-> { ` / `-> $x { ` wrapper).

Both fields render into `.message` as a numbered listing under a
`compiled block (template line N → block line M):` header, so the user sees
their template line *and* the surrounding closure shape the renderer
generated. With `trace: False` (the default) the message is unchanged.

```raku
my $cfg = Template::HAML::Config.new(:trace);
HAML.render(:src("= no_such_sub()\n"), :config($cfg));
# X::HAML::Eval whose .message ends with:
#   compiled block (template line 1 → block line 2):
#     1 | -> {
#     2 | no_such_sub()
#     3 | }
```

`trace` is independent of `suppress-eval`: when eval is suppressed, no eval
happens and the breadcrumb has nothing to attach to.

## suppress-eval

When `suppress-eval: True`, the renderer does not call `EVAL` for `=`/`-`/`!=`/`&=`
lines. The line's children are still rendered (so structural tags around
suppressed expressions still appear). Use this for templates where the embedded
expressions are not trusted.

## Filter rendering: `cdata` and `mime-type`

The `:javascript` and `:css` filters look at two config options when emitting
their wrapper tags:

* When format is `xhtml`, or when `cdata: True` is set explicitly, the body is
  wrapped in a `CDATA` marker:

    ```html
    <script type='text/javascript'>
      //<![CDATA[
      alert(1);
      //]]>
    </script>
    ```

    `:css` uses the comment-wrapped form `/*<![CDATA[*/ ... /*]]>*/`.

* `mime-type` sets the `type="…"` attribute on the emitted `<script>` /
  `<style>` tag. When unset, XHTML defaults to `text/javascript` /
  `text/css`; HTML5 omits the attribute.

## Encoding

The `encoding` option controls two things:

* The `encoding=` attribute emitted by `!!! XML`.
* How template sources are decoded when they arrive as bytes.

Recognized names (case-insensitive, with aliases):

| Canonical      | Aliases                  |
|----------------|--------------------------|
| `utf-8`        | `utf8`                   |
| `utf-16`       | `utf16`                  |
| `utf-16le`     |                          |
| `utf-16be`     |                          |
| `utf-32`       | `utf32`                  |
| `ascii`        | `us-ascii`               |
| `iso-8859-1`   | `latin1`, `latin-1`      |
| `windows-1252` | `cp1252`                 |

Unknown names raise an error at `Config.new` time. Aliases are normalized to
the canonical form, so `Config.new(:encoding<UTF8>).encoding` returns
`'utf-8'`.

### Input handling

`render(:src(...))` accepts either a `Str` or a `Blob`:

```raku
my Blob $bytes = "%p café\n".encode('iso-8859-1');
my $cfg = Template::HAML::Config.new(:encoding<iso-8859-1>);
HAML.render(:src($bytes), :config($cfg));   # "<p>café</p>\n"
```

* A `Str` is used as-is (Raku strings are already Unicode codepoints).
* A `Blob` is decoded using the configured encoding. If the bytes are not
  valid for that encoding, `X::HAML::EncodingError` is raised.
* A leading byte-order mark (U+FEFF) is stripped from both `Str` and decoded
  `Blob` input.

`render(:file(...))` always reads bytes from disk and runs them through the
same decode path, so the configured encoding governs file reads as well.

## `hyphenate-data-attrs`

When the `data:` or `aria:` shorthand expands a hash whose keys are
camelCase, setting `hyphenate-data-attrs: True` rewrites each key to
kebab-case:

```haml
%a{data: {fooBar: 1}}
```

renders as `<a data-fooBar='1'>` by default and `<a data-foo-bar='1'>` with
the option on. The conversion is recursive, so nested hashes have every level
hyphenated.
