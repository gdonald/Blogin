---
title: Server adapters
slug: adapters
order: 7
description: A server adapter bridges a concrete HTTP server to the framework. Its one job is the abstract contract: a request comes in, a status / headers / body go...
toc: true
---
A server adapter bridges a concrete HTTP server to the framework. Its one job is
the abstract contract: a request comes in, a status / headers / body go out.
Because every adapter wraps the same [endpoint](/docs/MVC-Keayl/middleware) app, swapping the
underlying server (production Cro, or the in-memory test driver) changes only
which adapter you construct.

## The adapter contract

`MVC::Keayl::Adapter` is a role carrying the built app and turning a
[`Request`](/docs/MVC-Keayl/request) into the finalized response tuple:

```perl6
my ($status, $headers, $body) = $adapter.handle($request);
```

`handle` calls the app and finalizes the [`Response`](/docs/MVC-Keayl/response), so a concrete
adapter only has to translate its server's native request and response types to
and from the framework's.

## Test adapter

`MVC::Keayl::Adapter::Test` drives the whole stack in memory (no socket), which
makes it the tool for testing controllers, middleware, and routes:

```perl6
use MVC::Keayl::Adapter::Test;

my $adapter = MVC::Keayl::Adapter::Test.new(:app($built-app));

my $response = $adapter.get('/users');
my $created  = $adapter.post('/users', :body('name=Ada'), :headers({ Host => 'example.com' }));
```

`request($method, $target, …)` is the general form; `get` / `post` / `put` /
`patch` / `delete` / `head` are shorthands. Each builds a `Request`, runs it
through the app, and returns the `Response` for inspection:

```perl6
$response.status;
$response.header('Content-Type');
$response.body;
```

Named arguments (`:headers`, `:body`, `:scheme`, `:remote-address`) are passed
straight through to the request.

## Cro adapter

`MVC::Keayl::Adapter::Cro` is the reference production adapter, backed by
[Cro](https://cro.raku.org/):

```perl6
use MVC::Keayl::Adapter::Cro;

my $adapter = MVC::Keayl::Adapter::Cro.new(
  :app($built-app),
  :host('0.0.0.0'),
  :port(8080),
);

$adapter.start;
# ... serve ...
$adapter.stop;
```

For each incoming Cro request the adapter builds a `Request` (method, target,
headers, body, and the client address), runs the app, and copies the resulting
status, headers, and body onto the Cro response. `Content-Length` is left to Cro,
which derives it from the serialized body.

The `:scheme` you give the adapter (`http` by default) tells requests whether
they arrived over TLS; behind a proxy, the request's own `X-Forwarded-Proto`
handling still applies.
