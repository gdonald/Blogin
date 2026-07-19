---
title: CLI
slug: cli
order: 19
description: Template::HAML ships with a haml script for rendering and checking templates from the shell.
toc: true
---
Template::HAML ships with a `haml` script for rendering and checking templates from the shell.

## Subcommands

| Command          | Purpose                                                     |
|------------------|-------------------------------------------------------------|
| `haml render`    | Render one or more HAML files to HTML.                      |
| `haml check`     | Parse-only sanity check; non-zero exit on parse failure.    |
| `haml fmt`       | Pretty-print a HAML file in [canonical form](/docs/Template-HAML/canonical-form). |
| `haml lint`      | Static analysis; non-zero exit on diagnostics.              |
| `haml help`      | Top-level help. Also `--help`, `-h`.                        |

## Rendering

```sh
haml render view.haml
```

Output goes to standard out. Multiple files are concatenated in the order given. To write the result to a file, pass `-o`:

```sh
haml render view.haml -o view.html
```

### Reading from stdin

A file argument of `-` reads HAML source from standard input. This composes with the usual flags:

```sh
echo '%p hi' | haml render -
cat view.haml | haml render --locals name=Alice -
```

`-` may appear at most once per invocation. Parse errors on stdin are reported against the label `<stdin>`.

### Passing locals

Use `--locals` with a comma-separated list of `key=value` pairs:

```sh
haml render greet.haml --locals name=Alice,greeting=hi
```

The values are made available as Raku scalars inside the template (e.g. `#{$name}`).

### Format and escaping

| Flag                  | Effect                                                  |
|-----------------------|---------------------------------------------------------|
| `--format html5`      | HTML5 output (default).                                 |
| `--format html4`      | HTML 4.01 Transitional.                                 |
| `--format xhtml`      | XHTML 1.0 Transitional; void elements emit `<br />`.    |
| `--escape-html`       | Escape HTML in `=` expressions (default).               |
| `--no-escape-html`    | Disable HTML escaping for `=` expressions.              |
| `--ugly`              | Single-line output with no inter-tag whitespace.        |
| `--stream`            | Stream output incrementally; one chunk per top-level node. |

### Streaming output

`--stream` writes HTML to its destination as each top-level node finishes
rendering, instead of buffering the whole document and writing once. It is
useful when piping into a slow consumer or when the document is large enough
that first-byte latency matters.

```sh
haml render --stream view.haml
haml render --stream view.haml -o view.html
echo '%p hi' | haml render --stream -
```

`--stream` accepts a single input and cannot be combined with `--watch`
or `--out-dir`. The same trade-offs that apply to the
[`render-supply` API](/docs/Template-HAML/api#haml-render-supply) apply here: trim markers,
`--ugly`, and `remove-whitespace` operate per chunk, not across the
full document.

### Rendering multiple files

File arguments may be globs (e.g. `'views/*.haml'`). The glob is expanded by `haml` itself when it contains `*` or `?`; quoting prevents the shell from expanding it first, which matters in watch mode (see below).

When more than one file is rendered, output goes to `--out-dir <dir>`. Each input is mirrored to `<dir>/<basename>.html`:

```sh
haml render 'views/*.haml' --out-dir build/
```

`--out-dir` and `-o` are mutually exclusive. With a single input, either works; with multiple inputs, only `--out-dir` is accepted.

### Watch mode

```sh
haml render view.haml -o view.html --watch
haml render 'views/*.haml' --out-dir build/ --watch
```

`--watch` does an initial render of every matching input, then re-renders any file that changes on disk. The watcher runs until interrupted (Ctrl-C). `--watch` requires `-o` or `--out-dir` — there is no stdout streaming form.

By default the watcher uses Raku's `IO::Notification` (FSEvents on macOS, inotify on Linux). On filesystems where notifications are unreliable (NFS, some VM shared folders), pass `--poll <ms>` to fall back to mtime polling:

```sh
haml render 'views/*.haml' --out-dir build/ --watch --poll 200
```

Watch mode does not accept a `-` (stdin) argument.

## Checking

```sh
haml check view.haml
```

`check` parses each file and reports `OK` for files that parse cleanly. It exits with a non-zero status if any file fails to parse, so it is suitable for use in pre-commit hooks or CI.

## Formatting

```sh
haml fmt view.haml
```

`fmt` pretty-prints each HAML file in [canonical form](/docs/Template-HAML/canonical-form) and writes the result to standard out. Use the following flags to change behavior:

| Flag           | Effect                                                                |
|----------------|-----------------------------------------------------------------------|
| `-o <path>`    | Write the formatted result to a file (single input only).             |
| `--in-place`   | Rewrite each input file with its canonical form.                      |
| `--check`      | Exit non-zero if any file differs from its canonical form. No output. |

`--check` mode is suitable for CI: it emits no stdout, writes a one-line diagnostic to stderr for each non-canonical file, and exits with status `1` when at least one file is out of form. `--in-place` and `-o` are mutually exclusive, and neither may be combined with `--check`.

The canonical form is idempotent: `haml fmt | haml fmt` always equals `haml fmt`. See the [canonical form spec](/docs/Template-HAML/canonical-form) for the full set of rules.

## Linting

```sh
haml lint view.haml
```

`lint` parses each HAML file and runs the registered lint rules over the parse tree and source. Diagnostics are written to standard out, one per line, in the form:

```
path:line:col: severity: message (rule-id)
```

A file argument of `-` reads source from standard input; the diagnostic path becomes `<stdin>`.

### Exit codes

| Code | Meaning                                                            |
|------|--------------------------------------------------------------------|
| `0`  | No diagnostics reported.                                            |
| `1`  | At least one diagnostic reported, or a file failed to parse.       |
| `2`  | Invocation error (bad option, missing file argument).              |

### Built-in rules

| Rule                  | What it catches                                                 |
|-----------------------|-----------------------------------------------------------------|
| `deprecated-syntax`   | Use of the dropped `:ruby` filter (suggests `:raku`).           |
| `malformed-indent`    | Trailing whitespace on a line, and whitespace-only blank lines. |
| `unused-locals`       | A key in `--locals` that the template never references.         |

### Declaring expected locals

The `unused-locals` rule needs to know which locals the template will receive. Pass the expected keys with `--locals` (the same syntax as `haml render`); values are accepted but ignored:

```sh
haml lint --locals name,age,role view.haml
```

A key listed in `--locals` that is never referenced as `$key` in the template produces an `unused-locals` warning. Without `--locals`, the rule sleeps.

### Extending the rule set

Library users can register additional rules:

```raku
use Template::HAML::Lint;

class MyRule does Template::HAML::Lint::Rule {
  method id(--> Str) { 'my-rule' }
  method check(:$tree, :$src, :%locals --> List) {
    # ... return a list of Template::HAML::Lint::Diagnostic
  }
}

Template::HAML::Lint::register-rule(MyRule.new);
```

Registered rules run in addition to the built-ins. `clear-rules()` removes them.

## Help

Each subcommand has its own help:

```sh
haml --help
haml render --help
haml check --help
haml lint --help
haml fmt --help
```
