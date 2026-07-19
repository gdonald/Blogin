---
title: Plugins
slug: plugins
order: 17
description: Template::HAML::Plugin is the public surface for bundling related hooks — visitors, tag transformers, filters, and the markdown backend — into a single...
toc: true
---
`Template::HAML::Plugin` is the public surface for bundling related hooks —
[visitors](/docs/Template-HAML/api#register-visitor), [tag transformers](/docs/Template-HAML/api#register-tag-transformer),
[filters](/docs/Template-HAML/filters), and the [markdown backend](/docs/Template-HAML/filters) — into a single
artifact that can be installed and removed atomically.

## Why bundle?

Each hook registry exposes its own `register-*` / `clear-*` subs. They work
fine for one-off use, but a plugin author shipping several related hooks
needs every hook installed before the first render and every hook removed
on uninstall. The `Plugin` class makes that bookkeeping the framework's
responsibility:

* `install` runs every registration sub in declaration order.
* `uninstall` removes exactly the hooks this plugin registered, by name.
* `clear-plugins` uninstalls every plugin in reverse-install order.

## Declaring a plugin

```raku
use Template::HAML::Plugin;

my $bootstrap = Template::HAML::Plugin::Plugin.new(
  :name<bootstrap-helpers>,
  :visitors([
    %( :name<inject-meta>, :handler(-> $tree { ... ; $tree }) ),
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

install-plugin($bootstrap);
# ... render with hooks active

uninstall-plugin($bootstrap);
```

Every visitor, tag-transformer, and filter entry must be a hash with `:name`
and `:handler` keys. Both are validated at `Plugin.new` time, so a malformed
manifest fails at construction rather than mid-render. Plugin-registered
visitors are always named (so uninstall can find them) — anonymous visitors
are only available through `Template::HAML::Visitor::register-visitor`
directly.

## Lifecycle guarantees

The plugin lifecycle is stable. These properties are public API and won't
change without a major version bump:

1. **Registration order is preserved.** Inside one plugin, visitors run in the
   order they appear in the `:visitors` list. Across plugins, visitors run in
   plugin-install order, then per-plugin order. The same applies to filter
   and tag-transformer registration: later wins on name collision.
2. **Install is idempotent.** Calling `install-plugin($p)` on an already
   installed plugin is a no-op. The plugin doesn't appear twice in
   `installed-plugins()`.
3. **Uninstall is idempotent.** Calling `uninstall-plugin($p)` on a
   not-installed plugin is a no-op.
4. **Uninstall is by name.** Uninstalling a plugin clears the hooks it
   registered by name. If a *different* plugin (or direct registration)
   later overrode that name, uninstalling the original plugin removes the
   currently-registered handler for that name. Plugins that share hook names
   are co-operating, not isolated.
5. **`clear-plugins()` uninstalls in reverse install order.** Returns the
   number of plugins it removed.
6. **Hooks observe the same pipeline order as before:** parse → visitors →
   tag transformers → render. Filters resolve per `:name` line at render
   time. Tag transformers and visitors run before code generation, so
   `compile-source-to-raku` and the on-disk cache reflect the transformed
   tree. Installing or uninstalling a plugin auto-invalidates the
   in-process direct-emit and compiled-`&fn` caches; call
   [`clear-compiled-cache`](/docs/Template-HAML/api#haml-clear-compiled-cache) if you also
   need the on-disk cached `.rakumod` artifacts rebuilt.

## Inspecting installed plugins

```raku
my @plugins = installed-plugins();        # list of Plugin objects
my @names   = installed-plugin-names();   # list of Str
my $maybe   = installed-plugin('card');   # Plugin or type object
my Int $n   = clear-plugins();            # uninstall all, return count
```

`installed-plugin($name)` returns the `Plugin` type object (undefined) when
no plugin with that name is installed — guard with `.defined`.

## What is *not* a plugin

* Render-time helpers — those go through the `:context` object. See
  [Render context](/docs/Template-HAML/context).
* Configuration overrides — use `Template::HAML::Config`. See
  [Configuration](/docs/Template-HAML/config).
* Per-template scope additions — use `:%locals` or `:context` on `render`.
