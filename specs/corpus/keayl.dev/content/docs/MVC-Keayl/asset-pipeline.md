---
title: Asset pipeline
slug: asset-pipeline
order: 13
description: The asset pipeline fingerprints assets with a content hash, records the mapping in a manifest, and resolves the view helpers through it so a changed asset...
toc: true
---
The asset pipeline fingerprints assets with a content hash, records the mapping
in a manifest, and resolves the view helpers through it so a changed asset gets a
new URL. It also builds import maps and serves digested assets with a long cache
lifetime.

## Fingerprinting and the manifest

`digest-for($content)` is the content hash; `digested-name('app.css', $content)`
inserts it before the extension (`app-<digest>.css`). A `Manifest` maps each
logical path to its digested name:

```perl6
use MVC::Keayl::Assets;

my $manifest = MVC::Keayl::Assets::Manifest.build('app/assets'.IO, output => 'public/assets'.IO);
$manifest.lookup('css/app.css');   # 'css/app-<digest>.css'
```

`build` walks the source tree, digests each file, writes the digested copy to the
output directory, and records the mapping. `to-json`/`from-json` persist the
manifest.

## Resolving in views

The asset helpers take a `resolver`. `manifest-resolver($manifest)` resolves a
logical name to its digested `/assets/...` path, passing absolute and external
URLs through unchanged:

```perl6
my &resolver = manifest-resolver($manifest);

stylesheet-link-tag('application', :&resolver);   # href="/assets/application-<digest>.css"
image-tag('logo.png', :&resolver);                 # src="/assets/logo-<digest>.png"
```

`set-asset-manifest($manifest)` registers a manifest globally, and
`digested-resolver` is a resolver that consults it, so helpers can resolve without
threading the manifest through each call:

```perl6
set-asset-manifest($manifest);
image-tag('logo.png', resolver => &digested-resolver);
```

A view defaults to `digested-resolver`, so once a manifest is registered the
helpers emit fingerprinted URLs with no per-call configuration. When no manifest
is registered the same resolver emits the plain `/assets/<path>` URL, which keeps
development output unfingerprinted until you precompile.

## Loading the manifest at boot

An application loads the precompiled manifest during boot. The `assets`
initializer reads `public/assets/manifest.json` when it exists, registers it with
`set-asset-manifest`, and points the asset root at `public/assets`. After a
`keayl assets-precompile` run the view helpers fingerprint URLs in production with
no extra configuration, and a deploy with new asset content serves new URLs that
defeat the browser cache.

Both paths come from the `assets` config, defaulting to `public/assets`:

```json
{
  "assets": {
    "public-root": "public/assets",
    "manifest": "public/assets/manifest.json"
  }
}
```

## Serving

`MVC::Keayl::Assets::Serving::AssetsController` serves digested files from the
configured root with `Cache-Control: public, max-age=31536000, immutable` (safe
because the digest changes when the content does). It rejects path traversal and
returns `404` for unknown assets:

```perl6
set-asset-root('public/assets'.IO);
```

## Import maps

An `ImportMap` pins module names to URLs. `pin` adds one (defaulting the URL to
`/assets/<name>.js`); `pin-all-from` pins every `.js` file under a directory:

```perl6
my $importmap = MVC::Keayl::Assets::ImportMap.new;
$importmap.pin('application', preload => True);
$importmap.pin('lodash', to => 'https://cdn/lodash.js');
$importmap.pin-all-from('app/javascript/controllers'.IO, under => 'controllers');
```

`javascript-importmap-tags($importmap)` emits the `<script type="importmap">` with
the import JSON and a `<link rel="modulepreload">` for each preloaded pin. Pass a
`manifest` to resolve the module URLs to their digested paths:

```perl6
javascript-importmap-tags($importmap, :$manifest);
```

## Precompiling

`keayl assets-precompile` builds the manifest and digested files, writing them and
a `manifest.json` to `public/assets`:

```
keayl assets-precompile
```

It reads from `app/assets` and reports the count, or fails when there is no asset
source directory.
