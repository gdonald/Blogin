---
title: Engines
slug: engines
order: 9
description: An engine is a self-contained application packaged to mount inside a host. It carries its own routes, controllers, view paths, and initializers under an...
toc: true
---
An engine is a self-contained application packaged to mount inside a host. It
carries its own routes, controllers, view paths, and initializers under an
isolated namespace, and the host mounts it at a path.

## Defining an engine

`MVC::Keayl::Engine` takes a namespace, the controllers it owns, its routes
block, and the view paths it contributes:

```perl6
use MVC::Keayl::Engine;

my $blog = MVC::Keayl::Engine.new(
  namespace    => 'Blog',
  controllers  => [Blog::PostsController, Blog::CommentsController],
  view-paths   => ['engines/blog/app/views'],
  routes-block => {
    get '/posts',     to => 'posts#index';
    get '/posts/:id', to => 'posts#show';
  },
);
```

`isolate-namespace` can also set the namespace after construction.

## Namespace isolation

Within the engine, a route target uses the short controller name. The engine
resolves it inside its namespace, so `to => 'posts#index'` dispatches to
`Blog::PostsController` (its `controller-path` is `blog/posts`). The full path
works too. The engine's controllers are dispatched only by the engine, never by
the host.

## Building the endpoint

`endpoint` builds a dispatcher for the engine's routes and controllers. It is an
endpoint like any other, so it can be called directly or mounted:

```perl6
$blog.endpoint.call($request);
```

## Mounting in a host

Mount the engine endpoint in the host's routes. Requests below the mount point
are rebased and handed to the engine:

```perl6
my $router = routes {
  root to => 'home#index';
  mount $blog.endpoint, at => '/blog';
};
```

A request for `/blog/posts/5` reaches the engine as `/posts/5`.

## Host overrides

A host route declared before the mount takes precedence, so the host can override
an engine route:

```perl6
routes {
  get '/blog/posts', to => 'marketing#featured';   # wins over the engine
  mount $blog.endpoint, at => '/blog';
}
```

Unmatched paths still fall through to the engine.

The host can also override an engine's views. Pass `view-path-overrides` when
building the endpoint, and those paths are searched before the engine's, so a
host template of the same name wins:

```perl6
$blog.endpoint(view-path-overrides => ['app/views/blog']);
```

`view-paths`, `helper-paths`, and `asset-paths` expose what the engine
contributes, and `append-view-paths` (and its helper/asset counterparts) add
more.

## Initializers

An engine registers initializers and runs them in order, for setup the engine
needs at boot:

```perl6
$blog.initializer('seed-cache', -> $engine { ... });
$blog.run-initializers;
```
