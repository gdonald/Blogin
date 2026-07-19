---
title: Caching & streaming
slug: caching
order: 25
description: fresh-when sets validators on the response and renders a 304 Not Modified when the request's If-None-Match/If-Modified-Since show the client already has...
toc: true
---
## Conditional GET

`fresh-when` sets validators on the response and renders a `304 Not Modified`
when the request's `If-None-Match`/`If-Modified-Since` show the client already
has the current version. `is-stale` is its inverse: it returns true when you
should render.

```perl6
method show {
  self.render('show') if self.is-stale(etag => $post, last-modified => $post.updated-at);
}
```

`etag-for` computes an ETag from a value, using its `cache-key` or `to-hash` when
available. ETags are weak by default (`W/"..."`); pass `weak => False` for a
strong ETag. Matching uses a weak comparison, so `W/` prefixes are ignored, and
`*` matches anything.

## Cache headers

`expires-in` sets `Cache-Control` with a max-age, `private` by default or
`public`, plus any extra directives. `expires-now` marks the response
uncacheable:

```perl6
self.expires-in(3600, public => True, must-revalidate => True);
# Cache-Control: public, max-age=3600, must-revalidate

self.expires-now;                  # Cache-Control: no-cache
self.expires-now(no-store => True); # Cache-Control: no-store
```

## Cache store

`MVC::Keayl::Cache` provides a `Store` role with a low-level key-value API shared
by every backend:

```perl6
$store.write('user/1', $user, expires-in => 300);
$store.read('user/1');
$store.exist('user/1');
$store.delete('user/1');

$store.fetch('user/1', { load-user(1) }, expires-in => 300);
```

`fetch` returns a cached value or computes, stores, and returns it. `force`
recomputes, `skip-nil` declines to store an undefined result, and
`race-condition-ttl` serves the stale value to other readers while one recomputes
an expired entry.

`increment` and `decrement` maintain counters, preserving an entry's expiry
window across updates. `read-multi` and `write-multi` work on several keys at
once, and `delete-matched` removes every key matching a string glob or a regex:

```perl6
$store.increment('visits', 1, expires-in => 3600);
$store.write-multi({ a => 1, b => 2 });
$store.read-multi('a', 'b');
$store.delete-matched('user/*');
```

Entries carry an optional `version`; a read with a mismatched `version` misses.
A store can take a `namespace` that prefixes its keys and a `default-expires-in`
applied when a write gives no expiry.

### Backends

- `MemoryStore` keeps entries in process, with an optional `max-entries` bound
  that evicts the least recently used entry.
- `FileStore` persists each entry under a `root` directory, surviving across
  instances.
- `NullStore` stores nothing, so `fetch` always recomputes. It is the no-op
  backend for test and development.
- `ExternalStore` delegates to an injected `client` shaped like a Redis or
  Memcache driver (`get`, `set` with a `ttl`, `del`, `keys`), serializing each
  entry through it.

## Fragment caching

A store's `fetch` backs view fragment caching. `cache-key` derives a key from
parts (an object contributes its own `cache-key`), under a `views/` namespace,
with an optional template `digest`:

```perl6
cache-key($post, digest => template-digest($source));   # views/posts/1-2021/<digest>
```

A view caches a fragment with `cache-fragment`, which computes the content once
per key through its configured store:

```perl6
$view.cache-fragment([$post], { render-the-fragment() });
```

## Rate limiting

`rate-limit` is a controller class method that caps how often a client may reach
an action, backed by a cache store. It registers a before-action that counts
requests per discriminator within a window and blocks once the count passes the
limit:

```perl6
PostsController.rate-limit(to => 100, within => 60);
```

It also has an `is rate-limit` trait form for the class header:

```perl6
class PostsController is MVC::Keayl::Controller is rate-limit(to => 100, within => 60) { }
```

By default the discriminator is the request's remote IP and an over-limit request
gets `429 Too Many Requests` with a `Retry-After` header set to the window. `by`
supplies a custom discriminator and `with` a custom over-limit handler; `store`
and `name` choose the backing store and the counter name:

```perl6
ApiController.rate-limit(
  to => 1000, within => 3600,
  by    => -> $controller { $controller.api-key },
  with  => -> $controller { $controller.head(503) },
  store => $cache,
);
```

## Streaming

A response body can be streamed instead of buffered. `stream` takes an iterable
of chunks (strings or blobs); `stream-chunks` yields them encoded, and
`is-streaming` reports whether a stream is set, for an adapter to write chunked:

```perl6
$response.stream(gather { for @rows { take row-html($_) } });
```

This is the pull-based primitive: the whole sequence is known up front. For a
push-based response, where an action writes chunks over time, see
[live streaming](/docs/MVC-Keayl/live-streaming).

## Range requests

`send-file` honours a `Range` request header, responding `206 Partial Content`
with a `Content-Range` header and the requested byte slice, and advertises
`Accept-Ranges: bytes`.
