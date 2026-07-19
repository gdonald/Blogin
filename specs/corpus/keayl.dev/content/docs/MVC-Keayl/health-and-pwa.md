---
title: Health checks & PWA
slug: health-and-pwa
order: 36
description: The framework ships two small controllers that a generated app wires up by default: a health-check endpoint for uptime monitors and load balancers, and a...
toc: true
---
The framework ships two small controllers that a generated app wires up by
default: a health-check endpoint for uptime monitors and load balancers, and a
pair of progressive-web-app endpoints for a web manifest and a service worker.

## Health check

`MVC::Keayl::HealthController` answers a single `show` action that returns `200`
with a minimal green page. It is meant for liveness probes, so it renders without
a layout or a database touch:

```perl6
use MVC::Keayl::HealthController;

get '/up', to => 'health#show';
```

A request to `/up` returns `200` when the process is up and serving. A load
balancer or uptime monitor can poll it without authentication.

## PWA manifest and service worker

`MVC::Keayl::PWAController` serves the two files a progressive web app needs. Its
`manifest` action renders a web app manifest as `application/manifest+json`, and
its `service-worker` action renders a minimal service worker as `text/javascript`:

```perl6
use MVC::Keayl::PWAController;

get '/manifest.json',     to => 'pwa#manifest';
get '/service-worker.js', to => 'pwa#service-worker';
```

The manifest sets `start_url` to `/`, `display` to `standalone`, and leaves
`icons` empty for the app to fill in. The generated service worker claims clients
on activate and skips waiting on install, so an updated worker takes over without
a reload.

### Naming the app

The manifest `name` comes from the controller's `app-name` attribute, which
defaults to `Keayl Application`. Pass it when registering the controller as an
instance to control the name shown when the app is installed:

```perl6
MVC::Keayl::PWAController.new(app-name => 'My Store');
```

The manifest's `short_name` is `Keayl`.

## In a generated app

`keayl new` registers both controllers in `config/application.raku` and wires the
three routes in `config/routes.raku`, so a fresh app responds at `/up`,
`/manifest.json`, and `/service-worker.js` with no extra setup.
