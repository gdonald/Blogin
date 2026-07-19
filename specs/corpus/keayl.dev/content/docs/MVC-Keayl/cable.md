---
title: WebSockets
slug: cable
order: 29
description: MVC::Keayl::Cable is an ActionCable-style abstraction for real-time messaging: a connection holds a client, channels group its subscriptions, and a...
toc: true
---
`MVC::Keayl::Cable` is an ActionCable-style abstraction for real-time messaging: a
connection holds a client, channels group its subscriptions, and a pub/sub
backend fans broadcasts out to everyone on a stream.

## Connections

`MVC::Keayl::Cable::Connection` represents one client. It carries the pub/sub
backend, a `sink` (the transport's function for sending a message to that client),
and `identifiers` (whatever you use to identify the client, such as the current
user or room):

```perl6
my $conn = MVC::Keayl::Cable::Connection.new(
  pubsub      => $pubsub,
  sink        => -> $message { $websocket.send($message) },
  identifiers => %( user-id => 7 ),
);
```

`add-subscription($channel)` registers a channel and subscribes it; `disconnect`
tears every subscription down.

### Identification and authentication

Declare the identifiers a connection carries with `identified-by`, and
authenticate in `connect`. `set-identifier` records a verified identity;
`reject-unauthorized-connection` refuses the connection. `open` runs `connect`,
catching a rejection, after which `is-rejected` reports the outcome. A declared
identifier reads back as a method:

```perl6
class AppConnection is MVC::Keayl::Cable::Connection {
  method connect {
    my $user = find-verified-user(self.identifiers<token>);
    $user ?? self.set-identifier('current-user', $user) !! self.reject-unauthorized-connection;
  }
}
AppConnection.identified-by('current-user');

$conn.open;
$conn.current-user;   # the verified identifier
```

## Channels

Subclass `MVC::Keayl::Cable::Channel`. Override `subscribed` to start streaming,
`unsubscribed` to clean up, and add a method per action a client can invoke:

```perl6
class ChatChannel is MVC::Keayl::Cable::Channel {
  method subscribed {
    self.stream-from('room:' ~ self.connection.identifiers<room>);
  }

  method speak(%data) {
    self.broadcast-to('room:' ~ self.connection.identifiers<room>, %data<message>);
  }
}
```

- `stream-from($stream)` subscribes the channel to a broadcasting; messages on
  that stream are transmitted to this connection's client.
- `transmit($data)` sends data straight to this client.
- `broadcast-to($stream, $data)` publishes to a stream, reaching every connection
  subscribed to it.
- `perform($action, %data)` dispatches a client message to an action method. Only
  methods you define on the channel are callable, so framework methods like
  `stream-from` cannot be invoked from the wire.

`unsubscribe` (run for you on `disconnect`) drops the channel's stream
subscriptions and calls `unsubscribed`.

### Rejecting a subscription

`subscribed` may call `reject` to refuse the subscription. A rejected channel is
not added to the connection, and any streams it set up are torn down:

```perl6
method subscribed {
  self.reject unless self.connection.current-user.can-access(self.params<room>);
}
```

### Streaming for a model and coders

`stream-for($target)` derives the stream name from a model (its
`broadcasting-for`, namespaced by the channel) instead of a raw string. A
`coder` encodes broadcasts and decodes received messages; `JsonCoder` serializes
to and from JSON:

```perl6
method subscribed {
  self.stream-for($room, coder => JsonCoder.new);
}
```

`broadcast-to-target($target, $data)` publishes to a model's broadcasting through
the server pub/sub (set with `set-cable-pubsub`).

### Periodic timers

`periodically` registers a recurring callback. `run-periodic-timers` fires the
registered timers (the transport drives this on the configured interval):

```perl6
class PresenceChannel is MVC::Keayl::Cable::Channel {
  method ping { self.transmit('ping') }
}
PresenceChannel.periodically('ping', every => 3);
```

## Broadcasting from models

A model that does `MVC::Keayl::Cable::Broadcasting::Broadcastable` broadcasts
updates to a stream through the configured server pub/sub.
`broadcast-append-to`, `broadcast-replace-to`, and `broadcast-remove-to` send a
payload naming the action, target, and content; `broadcast-to` sends an
arbitrary payload:

```perl6
class Post does Broadcastable { has $.id }

$post.broadcast-append-to($post, target => 'posts', content => $rendered-html);
```

The broadcast reaches the channels streaming that broadcasting, which transmit
it to their clients.

## Pub/sub backends

A backend is any `MVC::Keayl::Cable::PubSub`, a role with `subscribe($stream,
&callback)`, `unsubscribe($id)`, and `broadcast($stream, $message)`.
`MVC::Keayl::Cable::PubSub::InMemory` is the built-in, single-process backend; it
tracks subscribers per stream and fans a broadcast out to each one.

`MVC::Keayl::Cable::PubSub::External` shares broadcasts across processes by
delegating to a networked client (Redis- or PostgreSQL-shaped). The client
provides `subscribe($stream, &callback)`, `unsubscribe($id)`,
`publish($stream, $message)`, and `subscriber-count($stream)`:

```perl6
my $pubsub = MVC::Keayl::Cable::PubSub::External.new(client => $redis-client);
```
