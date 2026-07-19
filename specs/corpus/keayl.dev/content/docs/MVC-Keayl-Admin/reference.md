---
title: Configuration reference
slug: reference
order: 21
description: The methods called on MVC::Keayl::Admin to set the admin up, in one place. Each links to the page covering it in detail. The per-resource declarations...
toc: true
---
The methods called on `MVC::Keayl::Admin` to set the admin up, in one place. Each
links to the page covering it in detail. The per-resource declarations used
inside a `register` block are tabulated in [Registering resources](/docs/MVC-Keayl-Admin/registration).

## Mounting and configuration

| Method | Purpose |
| ------ | ------- |
| `endpoint()` | Returns the engine dispatcher for the host to `mount`. See [Mounting](/docs/MVC-Keayl-Admin/mounting). |
| `config()` | Returns the shared configuration object. |
| `configure(:mount-path, :site-title, :logout-path)` | Sets the mount path, site title, and navbar logout link path. See [Mounting](/docs/MVC-Keayl-Admin/mounting). |
| `registry()` | Returns the resource registry (`by-model`, `by-slug`, `all`). |

## Resources and pages

| Method | Purpose |
| ------ | ------- |
| `register(Model, { ... }, :slug, :singular, :plural, :per-page, :scope-counts)` | Registers a resource. See [Registering resources](/docs/MVC-Keayl-Admin/registration). |
| `page(slug, &block, :title, :group, :label, :priority, :icon, :hide)` | Registers a standalone page. See [Customization](/docs/MVC-Keayl-Admin/customization). |
| `path-for(name, *args)` | Builds the mount-prefixed URL for a resource action. See [Registering resources](/docs/MVC-Keayl-Admin/registration). |

## Navigation and dashboard

| Method | Purpose |
| ------ | ------- |
| `menu-link(:label, :url, :group, :priority, :icon, :external)` | Adds a custom menu link. See [Navigation](/docs/MVC-Keayl-Admin/navigation). |
| `menu-group-order(*groups)` | Orders the menu groups explicitly. See [Navigation](/docs/MVC-Keayl-Admin/navigation). |
| `dashboard-block(&block, :title)` | Adds a custom dashboard panel. See [Navigation](/docs/MVC-Keayl-Admin/navigation). |

## Authentication and authorization

| Method | Purpose |
| ------ | ------- |
| `authenticate-with($strategy)` | Installs an authentication strategy. See [Authentication](/docs/MVC-Keayl-Admin/authentication). |
| `authorize-with($policy)` | Installs an authorization policy. See [Authorization](/docs/MVC-Keayl-Admin/authorization). |

## Assets, theming, and localization

| Method | Purpose |
| ------ | ------- |
| `use-stylesheet($url)` | Layers a host stylesheet after the bundle. See [Assets](/docs/MVC-Keayl-Admin/assets). |
| `view-path($dir)` | Adds a host view path searched before the engine's. See [Customization](/docs/MVC-Keayl-Admin/customization). |
| `load-locales($dir)` | Loads locale files into the I18n backend. See [Customization](/docs/MVC-Keayl-Admin/customization). |
| `locale($code)` | Selects the active locale. See [Customization](/docs/MVC-Keayl-Admin/customization). |

## Generators

The `keayl-admin` command scaffolds the admin from the shell, not from Raku:

| Command | Purpose |
| ------- | ------- |
| `keayl-admin generate admin:install` | Mounts the engine and writes the initializer, dashboard, and auth stubs. |
| `keayl-admin generate admin <Model>` | Emits an explicit registration introspected from the model's schema. |

See [Installation](/docs/MVC-Keayl-Admin/installation).
