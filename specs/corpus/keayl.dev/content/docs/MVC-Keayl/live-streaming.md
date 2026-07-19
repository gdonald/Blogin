---
title: Live streaming & SSE
slug: live-streaming
order: 26
description: A live action writes a response body incrementally instead of returning it all at once. It is the push counterpart of the pull-based stream primitive: the...
toc: true
---
A live action writes a response body incrementally instead of returning it all at
once. It is the push counterpart of the pull-based
[`stream`](/docs/MVC-Keayl/caching#streaming) primitive: the action runs on its own thread and
writes chunks as they become available, while the adapter pulls them through and
sends them to the client.

## Live responses

`live` runs a block on a separate thread, passing the controller and a stream.
The block writes chunks with `write` and the framework closes the stream when the
block returns:

```perl6
method numbers {
  self.live(-> $controller, $stream {
    for 1..10 -> $n {
      $stream.write("$n\n");
    }
  });
}
```

The action returns as soon as the block is spawned, so the dispatch loop is not
blocked while the body streams. The response reports `is-live` and `is-streaming`,
and `live-promise` is the `Promise` that completes when the writer thread
finishes.

A string chunk is encoded as UTF-8; a `Blob` chunk is written through unchanged.
Writing to a closed stream raises `X::MVC::Keayl::Live::StreamClosed`.

## Client disconnects

When the client goes away, the adapter calls `disconnect` on the stream. The next
`write` then raises `X::MVC::Keayl::Live::ClientDisconnected`, which the action
catches to release resources and stop producing:

```perl6
method feed {
  self.live(-> $controller, $stream {
    CATCH {
      when X::MVC::Keayl::Live::ClientDisconnected { cleanup() }
    }

    loop {
      $stream.write(next-chunk());
    }
  });
}
```

`is-disconnected` reports whether the client has gone, and `disconnect` both marks
the stream and closes it.

## Server-Sent Events

`sse` wraps the live stream in an SSE writer. It sets the `text/event-stream`
content type and a `no-cache` directive, then passes the controller and an SSE
writer to the block:

```perl6
method events {
  self.sse(-> $controller, $sse {
    $sse.write('hello', event => 'greeting');
    $sse.write('world');
  });
}
```

`write` emits one event frame. `event`, `id`, and `retry` precede the data, and a
multiline payload is split across `data:` fields:

```
event: greeting
data: hello

data: world
```

Pass defaults to `sse` to apply them to every frame; a per-write option overrides
the default:

```perl6
self.sse(-> $controller, $sse {
  $sse.write('tick');                 # retry: 5000 applied
  $sse.write('pong', retry => 1000);  # overridden for this frame
}, retry => 5000);
```

`comment` writes an SSE comment line (`: text`), which serves as a heartbeat to
keep an idle connection alive without delivering an event:

```perl6
$sse.comment('keep-alive');
```
