---
title: API
slug: api
order: 18
description: Template::HAML exposes a layered API: a one-call render for most callers, streaming and cached-compile variants for performance, an optional direct-emit...
toc: true
---
`Template::HAML` exposes a layered API: a one-call `render` for most callers,
streaming and cached-compile variants for performance, an optional
direct-emit codegen path, and extension registries (filters, visitors, tag
transformers, plugins) for projects that need to customize the pipeline.

## `HAML.render`

```raku
use Template::HAML;
use Template::HAML::Config;

my Str $html = HAML.render(:src("..."));

my Str $html = HAML.render(
  :src("= \$name\n"),
  :locals(%(:name<World>)),
);

my $cfg = Template::HAML::Config.new(:format<xhtml>);
my Str $html = HAML.render(:src("%br\n"), :config($cfg));
```

| Parameter | Type    | Description                                                          |
|-----------|---------|----------------------------------------------------------------------|
| `:src`    | `Str:D` | The HAML source to parse and render.                                 |
| `:locals` | `%h`    | Optional name → value map; each key is bound as a `$name` lexical visible to embedded Raku in `=`/`-`/`!=`/`&=` lines. |
| `:config` | `Template::HAML::Config` | Optional rendering options. See [Configuration](/docs/Template-HAML/config). |
| `:context`| any     | Optional view-context object. Bare-identifier expressions resolve against its methods. Defaults to `Template::HAML::ViewContext.new`. See [Render context](/docs/Template-HAML/context). |

Returns the rendered HTML as a `Str`.

`HAML.render` may be called as either a class method (a fresh default config is
used) or an instance method (the instance's stored config is used unless
`:config` is passed at the call site). Construct an instance via
`HAML.new(:config(...))` to reuse the same config across renders.

## `HAML.render-supply`

```raku
use Template::HAML;

react {
  whenever HAML.render-supply(:src("%h1 hi\n%p there\n")) -> $chunk {
    $*OUT.print($chunk);
  }
}
```

Returns a `Supply[Str]` that emits HTML chunks as the renderer walks the
parse tree. Each top-level node in the source produces one chunk by
default, so output begins streaming before the entire template finishes
rendering.

| Parameter       | Type    | Description |
|-----------------|---------|-------------|
| `:src`          | `Str:D` / `Blob:D` | HAML source to render. Mutually exclusive with `:file`. |
| `:file`         | `Str:D` | Template file path (instance method only). Mutually exclusive with `:src`. |
| `:locals`       | `%h`    | Render-time locals; same semantics as `render`. |
| `:config`       | `Template::HAML::Config` | Optional rendering options. |
| `:context`      | any     | View-context object; same semantics as `render`. |
| `:flush-depth`  | `Int`   | Flush boundary depth. Default `1` (one chunk per top-level node). `0` emits the whole rendered output as a single chunk. |

### Trade-offs vs. `render`

`render-supply` walks the tree the same way `render` does, but commits
each chunk to the output as it goes. That means a few whole-document
post-processing passes are weakened or unavailable in streaming mode:

* **Trim markers (`>` / `<`)** apply only within a single chunk. A
  trim-outer on one top-level node cannot reach across the chunk
  boundary to chew whitespace produced by the next top-level node.
* **`output-style => 'ugly'`** runs the compression pass per chunk
  rather than once on the full document. The visible output is still
  one line per chunk, but the chunk boundary is preserved.
* **`remove-whitespace`** has the same per-chunk limitation as ugly
  mode.
* **`find-and-preserve`-style global rewrites** (any pass that needs
  the full output buffer in shape) are not run.

If a template relies on cross-document whitespace rewrites, use
`render` and write the full result, or set `:flush-depth(0)` to fall
back to a single-chunk emission.

## `HAML.compile-source-to-raku`

```raku
use Template::HAML;

my Str $raku-source = HAML.compile-source-to-raku(:src("%p hi\n"));
my &compiled        = EVAL $raku-source;
my Str $html        = compiled(%(:name<World>));
```

Compiles a HAML source string to a self-contained Raku source string. The
emitted source defines an anonymous sub with the signature

```raku
sub (%locals = (), Template::HAML::Config :$config, :$ctx --> Str)
```

When `EVAL`ed, the emitted source returns that sub. Calling it produces the
same HTML that `HAML.render(:src(...), :%locals, :config(...))` would.

| Parameter | Type    | Description                                                         |
|-----------|---------|---------------------------------------------------------------------|
| `:src`    | `Str:D` | The HAML source to compile.                                          |
| `:config` | `Template::HAML::Config` | Optional configuration baked into the emitted output. |

The emitted code rebuilds the parse tree with the per-class constructors in
`Template::HAML::*` and runs `Template::HAML::Renderer` on it, so feature
coverage matches the interpreter exactly. The `:$ctx` parameter (a
`Template::HAML::Context`) is plumbed through `$*HAML-CTX` so that helper
functions like `yield` and `render(:partial)` work from the compiled sub.

This is the substrate for cached/compiled templates. The emitted code
preserves source line and column information on every node, so eval
failures inside compiled templates raise an `X::HAML::Eval` whose `.line`
and `.column` point back to the originating HAML template — the same as
the interpreter.

## Direct-emit codegen (default)

`Template::HAML` ships with two codegen paths. The *direct-emit* path is now
the default: it emits Raku source that performs **inline string
concatenation** directly — no AST is reconstructed, and embedded Raku
expressions/control flow are inlined into the closure body rather than
re-`EVAL`ed per render. The legacy *AST-walker* path emits Raku source that
**rebuilds the parse tree** at render time and runs `Template::HAML::Renderer`
over it; it remains available as `:emit<ast>` and is what
[`compile-source-to-raku`](#haml-compile-source-to-raku) produces.

```raku
# Direct emit is the default — no :config needed.
my $html = HAML.render(:src("%p Hello, \#\{\$name}!\n"),
                       :locals(name => 'World'));

# Opt back into the AST renderer if you need a feature that's
# AST-only (see Feature parity below).
my $cfg  = Template::HAML::Config.new(:emit<ast>);
my $html = HAML.render(:src(...), :config($cfg));
```

The `:emit` config field accepts `'direct'` (default) or `'ast'`. The same
field flows through every entry point: `HAML.render`, `HAML.render-cached`,
`HAML.render-file-cached`. The cache key embeds `:emit`, so AST and direct
caches are stored under different filenames and coexist freely.

The `HAML_DEFAULT_EMIT` environment variable overrides the default for a
process: set it to `ast` to flip the default back to the AST walker (used
by the test suite to run every test under both paths).

### What direct emit changes

- Tags, plain text, comments, and doctypes become literal `$out ~= "…"`
  appends.
- `=`, `!=`, `&=`, `==`, `~`, and `-` statements are inlined as native Raku
  expressions in the closure body — no `EVAL` per render call. Bare
  identifier expressions still resolve through `eval-bare-ident` to keep
  context-method dispatch working.
- `- if`, `- elsif`, `- else`, `- unless`, `- for`, `- while`, `- repeat`,
  `- given`, `- when`, `- default` become native Raku `if/for/while/given`
  blocks.
- Dynamic attributes (interpolated values, `{|%splat}`, `[$obj-ref]`) build
  a `Tag` object once via `state` and call a shared
  `render-direct-tag-open` helper per render.
- Filter bodies are embedded as Raku string literals; dispatch goes through
  `lookup-filter($name)` at render time so user-registered filters still
  work.
- Every inlined Raku expression is wrapped in a `CATCH` that re-throws as
  `X::HAML::Eval` with the originating template line and column. Line
  mapping behaves the same as the AST path.

### Feature parity with the AST renderer

Direct emit is now at feature parity with the AST renderer for the
template-level features. The following all work on `:emit<direct>`:

- Tag `trim-outer` (`%div>`) and `trim-inner` (`%div<` / `<>`) modifiers.
- `remove-whitespace` config — applied as a virtual trim-outer/trim-inner on
  every non-preserved tag.
- Preserved tags (`<pre>`, `<textarea>`, custom names via `:preserve`) with
  children — inner whitespace is captured into a sub-buffer and encoded as
  `&#x000A;` after children render.
- `$*HAML-TAB-OFFSET` propagation — `tab-up`/`tab-down` helpers shift the
  static-tag indentation at runtime (indents are computed dynamically by
  the emitted code, not baked at codegen time).

Visitor and tag-transformer hooks also run as usual: they execute against
the parse tree before codegen, so the emitted source already reflects the
transformed tree regardless of `:emit` mode.

`find-and-preserve` is a render-time helper, not a post-render pass, so it
works inside `:emit<direct>` templates the same way it does in `:emit<ast>`
— call it on the relevant value before output.

### Streaming

`HAML.render-supply` does not yet support `:emit<direct>` — streaming stays
on the AST path even if the config requests `direct`. (The streaming
emitter is structured around walking the parse tree node-by-node, which is
exactly the thing direct emit eliminates.)

## On-disk compiled-template cache

The emitter from [`compile-source-to-raku`](#haml-compile-source-to-raku) can be
written to a per-user cache directory and loaded back on subsequent runs,
skipping the parse + emit step. This is opt-in; `HAML.render` does not
consult the cache.

Invalidation is automatic for both flavors of the API:

* String-based cache (`render-cached(:src)`): the cache key embeds a 64-bit
  hash of the source string, so changing the source produces a different key
  and a clean cache miss.
* File-based cache (`render-file-cached(:file)`): the cache key embeds the
  file's mtime, so editing the file produces a different key and a clean
  cache miss — no slurp + re-hash is needed on a cache hit.

The cache key also embeds the sorted list of helper names visible on the
`:context` object (see [Render context](/docs/Template-HAML/context)). The compiled
module is specialized to that helper set, so rendering the same source against
two contexts with different helpers produces two keys and two cached modules.
A render with no `:context` contributes an empty helper list, which is a
distinct key from any helper-bearing context.

Stale entries left behind on disk are not garbage-collected automatically; see
[`clear-compiled-cache`](#haml-clear-compiled-cache) below.

Each cache file is a self-contained Raku `unit module` exposing an
`our sub render`. The cache directory is registered as a
`CompUnit::Repository::FileSystem`, so first-load goes through Raku's
normal `require` pipeline — which means MoarVM precompiles the cached
module into `<cache-dir>/.precomp/` and reuses the bytecode across
processes. Subsequent fresh interpreters loading the same cache dir
skip the parse-and-compile of the generated Raku source as well.

On top of the on-disk cache, each unique `(src, config, helper-names)` or
`(file, mtime, config, helper-names)` tuple produces a compiled `&fn` closure
that is memoized in-process, so the cache file is loaded at most once per
process per template. See
[`compiled-fn-cache-size`](#haml-compiled-fn-cache-size) and
[`clear-compiled-fn-cache`](#haml-clear-compiled-fn-cache).

### `HAML.new(:compiled-cache-dir(...))`

```raku
my $haml = HAML.new(:compiled-cache-dir('/var/cache/Template-HAML'.IO));
```

Default cache directory resolution order:

1. The constructor's `:compiled-cache-dir` argument.
2. The `HAML_COMPILED_CACHE` environment variable.
3. `$*TMPDIR/Template-HAML/`.

### `HAML.compiled-cache-key`

```raku
my $key = $haml.compiled-cache-key(:src("%p hi\n"));
my $key = $haml.compiled-cache-key(:src(...), :config(...));
my $key = $haml.compiled-cache-key(:src(...), :context(...));
```

Returns a 16-character hex digest of `(source, config, helper-names)`, where
`helper-names` is the sorted set of helper methods on the optional `:context`.
Stable across processes; changes whenever any input changes.

### `HAML.compiled-cache-path`

```raku
my IO::Path $path = $haml.compiled-cache-path(:src(...), :config(...));
my IO::Path $path = $haml.compiled-cache-path(:src(...), :context(...));
```

Returns the absolute path where the compiled artifact for
`(src, config, helper-names)` would live. Layout: `<cache-dir>/Template/HAML/Compiled/T<key>.rakumod`.
The `T` prefix on the basename keeps the module name a valid Raku
identifier; the `Template/HAML/Compiled/` nesting matches the module name
so the cache dir can be used directly as a
`CompUnit::Repository::FileSystem` prefix. MoarVM stores precompiled
bytecode for each cached module under `<cache-dir>/.precomp/`.

### `HAML.compile-to-cache`

```raku
my IO::Path $path = $haml.compile-to-cache(:src(...), :config(...));
```

If the cache file already exists, returns its path unchanged (the file is
not rewritten). Otherwise, parses the source, emits Raku, writes it to the
cache path, and returns the path.

### `HAML.load-from-cache`

```raku
my &fn = $haml.load-from-cache($path);
my $html = fn(%locals, :config($cfg));
```

Loads the cached module (via `CompUnit::Repository::FileSystem.need`) and
returns its `&render` sub. The first load in a fresh process triggers
MoarVM precompilation under `<cache-dir>/.precomp/`; subsequent processes
that point at the same cache dir reuse that precompiled bytecode.

### `HAML.render-cached`

```raku
my $html = $haml.render-cached(:src(...), :%locals, :config(...));
```

Convenience wrapper: `compile-to-cache` + `load-from-cache` + invocation
with the appropriate `Template::HAML::Context` plumbed through `$*HAML-CTX`.

The compiled `&fn` is memoized in-process by cache key, so a cache file is
loaded at most once per process per `(src, config)` tuple. Subsequent calls
skip disk I/O entirely and reuse the in-memory closure. The memoization is
shared across `HAML` instances in the same process — different
`:compiled-cache-dir` values do not produce duplicate entries when the cache
key matches.

### File-based cache API

The same on-disk layout is reused for cached compilations keyed by file
path + mtime + config + context helper names, so editing the template file is
enough to invalidate.

```raku
my Str      $key  = $haml.compiled-cache-key-for-file(:file<views/home>);
my IO::Path $path = $haml.compiled-cache-path-for-file(:file<views/home>);
my IO::Path $path = $haml.compile-file-to-cache(:file<views/home>);
my Str      $html = $haml.render-file-cached(:file<views/home>, :%locals);
```

`compiled-cache-key-for-file` and `compiled-cache-path-for-file` accept
`:context`, mirroring the string-based `compiled-cache-key` and
`compiled-cache-path`: the context's helper names feed into the key.

File names are resolved through `:search-paths` and the same extension
fallbacks as `HAML.render(:file)` (bare name, `.haml`, `.html.haml`, partial
`_name` variants). The cache key changes whenever the resolved file's mtime
changes, so touching the file or editing it both force a recompile.

`render-file-cached` plumbs `:current-dir` through to the
`Template::HAML::Context` so that `render(:partial)` inside the template
resolves relative paths the same way `HAML.render(:file)` does.

Both `render-cached` and `render-file-cached` accept `:context`, and the
context's helper names participate in the cache key. The same template
rendered against contexts with different helper sets compiles and caches
separately, so a helper is never resolved against a module compiled for a
context that lacked it. When a `:layout` is given, the same helper set is
applied to both the inner template and the layout compilations.

Like `render-cached`, the compiled `&fn` is memoized in-process by cache key
(which includes the file's mtime). An edited file produces a new key and a
fresh load through `require`; an unchanged file reuses the in-memory closure.

`render-file-cached` also accepts `:layout`, mirroring `HAML.render(:file, :layout)`:

```raku
my Str $html = $haml.render-file-cached(
  :file<home>,
  :layout<layouts/app>,
  :%locals,
);
```

Both the inner template and the layout are compiled, cached on disk, and
memoized in-process independently. Editing either file invalidates only that
file's cache entry. The layout sees the inner render through `= yield` (and
`yield(:name<...>)` for named blocks via `content-for`), exactly as the
non-cached `render(:file, :layout)` path does.

### `HAML.clear-compiled-cache`

```raku
my Int $removed = $haml.clear-compiled-cache;
```

Removes every cached `.rakumod` file under `<cache-dir>/Template/HAML/Compiled/`
and deletes the `<cache-dir>/.precomp/` tree, then returns the count of
removed cache modules. Empty `Template/HAML/Compiled/` parent directories
are pruned too. Use this when configuration changes, after upgrading
Template::HAML, or any time the cache is suspected of being stale. The
in-process `&fn` memoization is cleared as well.

### `HAML.compiled-fn-cache-size`

```raku
my Int $n = $haml.compiled-fn-cache-size;
```

Returns the number of compiled `&fn` closures currently memoized in-process.
The memoization is process-wide (shared across `HAML` instances), so this is
also the global count.

### `HAML.clear-compiled-fn-cache`

```raku
my Int $cleared = $haml.clear-compiled-fn-cache;
```

Drops every in-process memoized `&fn` and returns the number of entries that
were removed. The on-disk cache is untouched; the next render rebuilds the
in-memory entry by `require`ing the cache module (or recompiling if the file
is missing). The MoarVM precomp cache under `<cache-dir>/.precomp/` is also
left in place, so the re-load is bytecode-fast.

## `register-filter`

```raku
use Template::HAML::Filters;

register-filter :name<upper>, :handler(-> Str $body, %locals --> Str {
  $body.uc;
});
```

Registers a custom filter handler. The handler signature is
`(Str $body, %locals --> Str)`. `$body` is the filter's indented block
dedented to column zero; the handler returns the rendered text. The renderer
applies the filter's own source indent to each line of the result.

`Template::HAML::Filters` also exports:

| Sub                                       | Description                                                              |
|-------------------------------------------|--------------------------------------------------------------------------|
| `lookup-filter(Str $name)`                | Returns the handler for a filter name, or undefined.                     |
| `has-filter(Str $name --> Bool)`          | Whether a filter is registered with that name.                           |
| `filter-names(--> Seq)`                   | Sorted list of every registered filter name.                             |
| `filter-count(--> Int)`                   | Number of registered filters (built-in + user-registered).               |
| `clear-filter(Str:D $name --> Bool)`      | Remove a single filter by name; returns whether one was removed. The built-in filters live in the same registry; clearing one of them is permitted but rarely useful. |

See [filters](/docs/Template-HAML/filters) for the built-in handlers.

## `register-visitor`

```raku
use Template::HAML;
use Template::HAML::Visitor;
use Template::HAML::Tag;

register-visitor :name<rename-b>, :handler(-> $tree {
  walk-tree($tree, -> $node {
    if $node.object ~~ Tag && $node.object.name eq 'b' {
      $node.object.name = 'strong';
    }
  });
  $tree;
});

HAML.render(:src("%b hi\n"));  # → "<strong>hi</strong>\n"
```

Registers an AST visitor that runs against the parse tree of every subsequent
`HAML.render` (and every other compile path: `render-cached`,
`compile-source-to-raku`, `compile-file`). The handler signature is

```raku
sub (Template::HAML::Node $tree --> Template::HAML::Node)
```

The handler may mutate the tree in place and return it, or return a fresh
`Template::HAML::Node` to replace it entirely. When multiple visitors are
registered they run in registration order and each one's return value is
fed into the next.

| Parameter   | Type      | Description                                                  |
|-------------|-----------|--------------------------------------------------------------|
| `:handler`  | `Callable`| Tree transformer (required).                                 |
| `:name`     | `Str`     | Optional. Registering with an existing name replaces the previous handler. Anonymous visitors always append. |

`Template::HAML::Visitor` also exports:

| Sub                              | Description                                                                  |
|----------------------------------|------------------------------------------------------------------------------|
| `clear-visitors(--> Int)`        | Remove every registered visitor; returns the number removed.                |
| `clear-visitor(Str:D $name --> Bool)` | Remove a named visitor; returns whether one was removed.                |
| `has-visitor(Str:D $name --> Bool)`   | Whether a visitor with that name is registered.                        |
| `visitor-names()`                | List of registered visitor names (anonymous visitors are not included).      |
| `visitor-count(--> Int)`         | Total registered visitors, named and anonymous.                              |
| `walk-tree(Node:D $tree, &cb)`   | Depth-first walk of every `Template::HAML::Node` reachable from `$tree`.    |
| `apply-visitors(Node:D $tree --> Node)` | Run every registered visitor against `$tree` in order. Called automatically during compilation; exported for tests. |

Visitors run between parsing and rendering, so the renderer always operates
on the post-visitor tree. They also run before [code generation](#haml-compile-source-to-raku),
which means the resulting cached `.rakumod` reflects the transformed tree.
Registering, replacing, or clearing a visitor automatically invalidates the
in-process direct-emit and compiled-`&fn` caches; the on-disk cache is left
in place — call [`clear-compiled-cache`](#haml-clear-compiled-cache) if you
also need the cached `.rakumod` artifacts rebuilt.

## `register-tag-transformer`

```raku
use Template::HAML;
use Template::HAML::TagTransformers;

register-tag-transformer :name<card>, :handler(-> $node {
  $node.object.name = 'div';
  $node.object.attrs.push: 'class' => 'card';
  $node;
});

HAML.render(:src("%card hi\n"));  # → "<div class='card'>hi</div>\n"
```

Registers a transformer for a specific tag name. Each `Node` in the parse
tree whose object is a `Template::HAML::Tag` with that name is passed to
the handler. The handler signature is

```raku
sub (Template::HAML::Node $node --> Template::HAML::Node)
```

The handler may either mutate the tag in place and return the same node,
or return a fresh `Template::HAML::Node` to replace it entirely (in which
case any original children are *not* re-attached automatically — the
handler must preserve them if needed). Returning `Nil` keeps the original
node unchanged.

Tag transformers run after the [visitor pass](#register-visitor) but before
rendering and code generation, so the cached `.rakumod` reflects the
expanded form. Registering, replacing, or clearing a transformer auto-invalidates
the in-process direct-emit and compiled-`&fn` caches; call
[`clear-compiled-cache`](#haml-clear-compiled-cache) if you also need the cached
`.rakumod` artifacts rebuilt.

| Parameter   | Type      | Description                                                  |
|-------------|-----------|--------------------------------------------------------------|
| `:name`     | `Str:D`   | Tag name to dispatch on (required). Registering with an existing name replaces the previous handler. |
| `:handler`  | `Callable`| `Node` transformer (required).                              |

`Template::HAML::TagTransformers` also exports:

| Sub                                          | Description                                                              |
|----------------------------------------------|--------------------------------------------------------------------------|
| `clear-tag-transformers(--> Int)`            | Remove every registered transformer; returns the number removed.        |
| `clear-tag-transformer(Str:D $name --> Bool)`| Remove a named transformer; returns whether one was removed.            |
| `has-tag-transformer(Str:D $name --> Bool)`  | Whether a transformer is registered for that tag name.                  |
| `tag-transformer-names()`                    | List of registered tag names, in registration order.                    |
| `tag-transformer-count(--> Int)`             | Total registered transformers.                                          |
| `lookup-tag-transformer(Str:D $name)`        | Returns the handler for a tag name, or `Nil` if none is registered.     |
| `apply-tag-transformers(Node:D $tree --> Node)` | Run dispatch over every matching `Tag` in `$tree`. Called automatically during compilation; exported for tests. |

## `Template::HAML::Plugin`

`Template::HAML::Plugin` is the public, stable surface for bundling related
hooks — visitors, tag transformers, filters, and the markdown backend — so a
plugin author can install or remove them atomically. The individual
`register-*` subs documented above remain available; `Template::HAML::Plugin`
adds a lifecycle layer on top.

```raku
use Template::HAML::Plugin;

my $plugin = Template::HAML::Plugin::Plugin.new(
  :name<my-plugin>,
  :visitors([
    %( :name<add-class>, :handler(-> $tree { ...; $tree }) ),
  ]),
  :tag-transformers([
    %( :name<card>, :handler(-> $node {
      $node.object.name = 'div';
      $node.object.attrs.push: 'class' => 'card';
      $node;
    }) ),
  ]),
  :filters([
    %( :name<upper>, :handler(-> Str $body, %locals --> Str { $body.uc }) ),
  ]),
  :markdown-backend(-> Str $body --> Str { ... }),
);

install-plugin($plugin);
# ... render with hooks active

uninstall-plugin($plugin);
```

| Plugin attribute     | Type                                       | Description                                                              |
|----------------------|--------------------------------------------|--------------------------------------------------------------------------|
| `:name`              | `Str:D` (required)                         | Identifies the plugin in `installed-plugins`.                            |
| `:visitors`          | `Array` of `%(:name, :handler)` hashes     | One entry per `register-visitor` call.                                   |
| `:tag-transformers`  | `Array` of `%(:name, :handler)` hashes     | One entry per `register-tag-transformer` call.                           |
| `:filters`           | `Array` of `%(:name, :handler)` hashes     | One entry per `register-filter` call.                                    |
| `:markdown-backend`  | `Callable`                                 | Optional; installs as the global markdown backend.                       |

Each entry hash is validated at `Plugin.new` time — missing `:name` or
`:handler` raises immediately rather than at install time. Plugin-registered
visitors are always named (anonymous visitors are only available through
`Template::HAML::Visitor::register-visitor` directly).

`Template::HAML::Plugin` exports:

| Sub                                       | Description                                                              |
|-------------------------------------------|--------------------------------------------------------------------------|
| `install-plugin(Plugin:D $p --> Plugin)`  | Registers every hook the plugin declares. Idempotent.                    |
| `uninstall-plugin(Plugin:D $p --> Plugin)`| Clears every hook by name. Idempotent.                                   |
| `installed-plugins()`                     | List of currently installed `Plugin` objects.                            |
| `installed-plugin-names()`                | List of installed plugin names.                                          |
| `installed-plugin(Str:D $name)`           | Returns the installed `Plugin` with that name, or the type object.       |
| `clear-plugins(--> Int)`                  | Uninstalls every plugin in reverse install order. Returns the count.     |

See [Plugins](/docs/Template-HAML/plugins) for the full lifecycle guarantees, cache
considerations, and design rationale.

## Internal modules

The implementation is split across several modules under `lib/Template/HAML/`. These are not part of the stable public API yet, but documented here for contributors:

| Module                       | Responsibility                                                |
|------------------------------|---------------------------------------------------------------|
| `Template::HAML::Grammar`    | The Raku Grammar that recognizes HAML source.                 |
| `Template::HAML::Actions`    | Builds the parse tree from grammar matches.                   |
| `Template::HAML::Node`       | Generic tree node holding a `Tag` or `Statement` payload.     |
| `Template::HAML::Tag`        | AST node representing a single HAML tag.                      |
| `Template::HAML::Statement`  | AST node representing an embedded-code line (`=`, `-`, `!=`, `&=`). |
| `Template::HAML::Eval`       | EVALs embedded Raku expressions with caching.                 |
| `Template::HAML::Multiline`  | Pre-grammar pass that joins continued code lines (trailing comma / unbalanced brackets). |
| `Template::HAML::Renderer`   | Walks the parse tree and emits HTML.                          |
| `Template::HAML::Codegen`    | Emits Raku source that reconstructs the parse tree and renders it (`:emit<ast>`). |
| `Template::HAML::DirectCodegen` | Emits Raku source that performs inline string concatenation (`:emit<direct>`, the default). |
| `Template::HAML::Cache`      | Cache-key hashing and on-disk layout for compiled templates.        |
| `Template::HAML::Filter`     | AST node representing a filter line and its dedented body.    |
| `Template::HAML::Filters`    | Filter registry plus the built-in filter handlers.            |
| `Template::HAML::Config`     | Per-render configuration: format, escape options, output style, etc. |
| `Template::HAML::Visitor`    | AST visitor registry; transforms the parse tree before render/codegen. |
| `Template::HAML::TagTransformers` | Per-tag-name transformer registry; rewrites individual tags before render/codegen. |
| `Template::HAML::Plugin`     | Public plugin lifecycle: bundles visitors, tag transformers, filters, and the markdown backend behind atomic install/uninstall. |
| `Template::HAML::Helpers`    | Built-in helper subs (`html-safe`, `surround`, `list-of`, `yield`, …) reachable from embedded code. |
| `Template::HAML::HelpersRole`| Role version of the helpers, for composition into a custom render context. |
| `Template::HAML::ViewContext`| Default render-context class; composes `HelpersRole` so helpers resolve as bare identifiers. |
| `Template::HAML::Context`    | Per-render bookkeeping: `yield`/`content-for` slots, partial depth, current dir. |
| `Template::HAML::Format`     | `haml fmt` emitter — pretty-prints a parsed template in canonical form. |
| `Template::HAML::Lint`       | `haml lint` rule registry, built-in rules, and `Diagnostic` formatter. |
| `Template::HAML::Watch`      | File-watch loop used by `haml render --watch`. |
| `Template::HAML::CLI`        | Subcommand dispatch and option parsing for the `haml` script. |
| `Template::HAML::X`          | Exception types raised by the parser.                         |

## Exceptions

Every `X::HAML::*` subclass inherits from `X::HAML` and carries the
following attributes when raised:

| Attribute  | Description                                            |
|------------|--------------------------------------------------------|
| `line`     | 1-based line number where the error was detected.     |
| `column`   | 1-based column within that line.                       |
| `file`     | Source file path, when `HAML.render(:file(...))` was used. |
| `snippet`  | The source line containing the error, used for the caret pointer. |

The `.message` of every subclass includes a `[HAML <file>:<line>:<col>]`
prefix and (when a snippet was captured) a one-line caret-pointer that
underlines the offending column:

```
[HAML source:3:1] mixed tabs and spaces in indent
  3 | 	%b bye
    | ^
```

| Exception                    | When                                                       |
|------------------------------|------------------------------------------------------------|
| `X::HAML::IllegalIndent`     | A line's leading whitespace isn't a valid indent.          |
| `X::HAML::IndentMixed`       | Tabs and spaces are combined in one indent.                |
| `X::HAML::IndentInconsistent`| Indent isn't a multiple of the first observed unit.        |
| `X::HAML::DuplicateId`       | A tag has both `#id` shorthand and an `id:` attr.          |
| `X::HAML::VoidWithChildren`  | A void element (`br`, `img`, …) was given child nodes.     |
| `X::HAML::ParseFail`         | The source did not parse; `snippet` is the failing line.   |
| `X::HAML::UnknownDoctype`    | `!!! foo` named a doctype variant that is not recognized.  |
| `X::HAML::DoctypeNotFirst`   | A `!!!` line appeared after non-blank content.             |
| `X::HAML::Eval`              | An embedded `=`/`-`/`!=`/`&=` expression failed to compile or run. |
| `X::HAML::UnbalancedExpression` | A multi-line code expression ran to end of source with open brackets or a trailing comma. |
| `X::HAML::UnknownFilter`     | A `:name` line referenced a filter that is not registered. |
| `X::HAML::MarkdownBackendMissing` | A `:markdown` filter rendered but no backend was registered via `register-markdown-backend`. |
| `X::HAML::OrphanElse`        | An `- elsif`/`- else` had no preceding `if`/`unless`.      |
| `X::HAML::InvalidEncoding`   | `Template::HAML::Config.new(:encoding(...))` was given an unknown encoding name. |
| `X::HAML::EncodingError`     | A `Blob` source (or file read) could not be decoded with the configured encoding. |
| `X::HAML::TemplateNotFound`  | `HAML.render(:file(...))` could not locate the template.   |
| `X::HAML::PartialDepthExceeded` | A partial recursed past the configured depth limit.     |
| `X::HAML::YieldOutsideLayout`| `yield()` was called outside a layout rendering context.   |

### Debug logging

`Template::HAML::X` exports `haml-debug(*@msg)` for low-noise diagnostics.
It is a no-op unless the `HAML_DEBUG` environment variable is set, in
which case each call writes a `[HAML DEBUG]`-prefixed line to STDERR.
Use it sparingly inside the implementation; it must never be left on a
hot path.
