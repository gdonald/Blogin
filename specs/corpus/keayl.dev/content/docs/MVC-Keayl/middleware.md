---
title: Middleware
slug: middleware
order: 6
description: MVC::Keayl processes a request through a stack of middleware wrapped around a single innermost endpoint. The model mirrors Rack: each middleware receives...
toc: true
---
MVC::Keayl processes a request through a stack of middleware wrapped around a
single innermost **endpoint**. The model mirrors Rack: each middleware receives
the downstream app, can act before and after delegating to it, and may
short-circuit by returning its own response.

## The endpoint protocol

An endpoint is anything that does the `MVC::Keayl::Endpoint` role: a single
`call` method that takes a [`Request`](/docs/MVC-Keayl/request) and returns a
[`Response`](/docs/MVC-Keayl/response):

```perl6
use MVC::Keayl::Endpoint;
use MVC::Keayl::Request;
use MVC::Keayl::Response;

class Hello does MVC::Keayl::Endpoint {
  method call(MVC::Keayl::Request:D $request --> MVC::Keayl::Response:D) {
    MVC::Keayl::Response.new(:body('Hello'));
  }
}
```

The router (see Routing) is the endpoint that sits innermost in the stack.

## Writing middleware

A middleware subclasses `MVC::Keayl::Middleware`. The base class holds the
downstream app in `app` and, by default, passes the request straight through.
Override `call` to wrap that delegation:

```perl6
use MVC::Keayl::Middleware;

class Timing is MVC::Keayl::Middleware {
  method call(MVC::Keayl::Request:D $request --> MVC::Keayl::Response:D) {
    # ... before ...
    my $response = self.app.call($request);
    # ... after ...
    $response;
  }
}
```

A middleware that returns a response **without** calling `self.app` short-circuits
the rest of the stack, useful for authentication gates, caching, and the like.

## The stack

`MVC::Keayl::MiddlewareStack` is an ordered, named collection of middleware. The
first entry added is the **outermost** (it sees the request first and the
response last).

```perl6
use MVC::Keayl::MiddlewareStack;

my $stack = MVC::Keayl::MiddlewareStack.new;

$stack.use('logger', Logging);
$stack.use('timing', Timing);
```

Construction arguments after the class are forwarded to the middleware's `new`
alongside `app`:

```perl6
$stack.use('host-guard', HostAuthorization, :allowed<example.com>);
```

### Reordering

Entries are addressed by name:

```perl6
$stack.insert-before('timing', 'request-id', RequestId);
$stack.insert-after('logger', 'ssl', ForceSSL);
$stack.delete('timing');
```

`insert-before` and `insert-after` raise if the named target is not in the stack.

### Introspection

```perl6
$stack.names;            # ('logger', 'timing'), in order
$stack.elems;            # 2
$stack.contains('ssl');  # False
```

### Building the app

`build` wraps an endpoint with the whole stack and returns the resulting app,
itself an endpoint you `call`:

```perl6
my $app = $stack.build($router);

my $response = $app.call($request);
```

An empty stack returns the endpoint unwrapped.

## Serving static files

`MVC::Keayl::Middleware::Static` serves files from a directory and passes every
other request through to the app. It suits development and small deployments; in
production a front-end server such as Apache or nginx usually serves the files
directly.

```perl6
use MVC::Keayl::Middleware::Static;

$stack.prepend('static', MVC::Keayl::Middleware::Static, root => 'public'.IO);
```

A request maps to a file beneath `root`, so `GET /css/app.css` serves
`public/css/app.css`. Only `GET` and `HEAD` are handled; any other method, a
missing file, or a directory passes through to the app. The content type is
derived from the file extension, falling back to `application/octet-stream`.
Parent-directory segments are refused, so a request cannot escape `root`.

`url-prefix` serves a directory beneath a path prefix, stripping it before the
lookup. With the settings below, `GET /assets/css/app.css` serves
`assets/css/app.css`, while a path outside the prefix passes through:

```perl6
$stack.prepend('static', MVC::Keayl::Middleware::Static,
  root       => 'assets'.IO,
  url-prefix => '/assets');
```

Prepending keeps it outermost, so a matched file short-circuits before routing.

## Per-request database connections

`MVC::Keayl::Middleware::Database` gives each request its own pooled connection
per database it touches. When a database is configured, the application prepends
it automatically through the `active-record-connection` initializer, so you do
not wire it by hand.

It binds a request-scoped connection registry for the duration of the request.
The first model query on a connection checks one out of that connection's pool
and caches it; every later query on the same connection reuses it, so the whole
request sees a consistent connection and transaction. The connections return to
their pools when the request ends, including when the request raises.

```perl6
use MVC::Keayl::Middleware::Database;

$stack.prepend('db-connection', MVC::Keayl::Middleware::Database);
```

The registry is keyed by connection name, so an app with a read replica or
several databases routes each model to its own pooled connection rather than
forcing everything onto one. The pool verifies a connection on checkout and
reconnects a dropped one, so a socket that died between requests heals on the
next request instead of failing every query until the server restarts.
