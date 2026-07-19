---
title: Instrumentation
slug: instrumentation
order: 34
description: MVC::Keayl::Notifications is a pub/sub bus modeled on ActiveSupport::Notifications. The framework instruments the points it controls, subscribers receive...
toc: true
---
`MVC::Keayl::Notifications` is a pub/sub bus modeled on
`ActiveSupport::Notifications`. The framework instruments the points it controls,
subscribers receive each event's payload, and a request id ties a request's
events together.

## Subscribing

State is process-wide, so subscribe once at boot. A subscriber is a callback that
receives the event's payload hash.

```perl6
MVC::Keayl::Notifications.subscribe('dispatch.keayl', -> %payload {
  say "{%payload<controller>}#{%payload<action>} took {%payload<duration>}s";
});
```

`subscribe` returns an id for `unsubscribe`. `has-subscribers($event)` reports
whether anyone is listening, and `reset` clears every subscription (useful
between tests).

## Emitting events

`notify($event, %payload)` fires an instantaneous event. `instrument($event,
%payload, &block)` runs the block, times it, and notifies subscribers with
`duration` (seconds) merged into the payload, returning the block's result. When
the block throws, subscribers still see the event with the exception in
`exception`, and the error is rethrown.

Both short-circuit when nothing is subscribed to the event, so instrumentation
left in place costs a hash lookup when no one is listening.

## Framework events

| Event             | Emitted around              | Payload keys                                   |
| ----------------- | --------------------------- | ---------------------------------------------- |
| `dispatch.keayl`  | a controller action         | `controller`, `action`, `method`, `path`, `request-id`, `duration` |
| `render.keayl`    | a template / partial render | `kind`, `name`, `duration`                     |

`kind` is one of `template`, `inline`, `partial`, or `object`.

## Database events

`ORM::ActiveRecord` publishes `sql.active_record` on its own notifications bus.
Bridge it onto the framework bus so DB queries arrive alongside dispatch and
render events:

```perl6
use ORM::ActiveRecord::Instrumentation::Notifications;

MVC::Keayl::Notifications.bridge(Notifications);
```

`bridge($source, $event = 'sql.active_record')` subscribes to the source bus and
re-emits each event onto the framework bus, so one subscriber sees the whole
request.

## Request ids

`MVC::Keayl::Middleware::RequestId` gives every request an id, wired into the
application by default. It reuses a valid incoming `X-Request-Id` header (word
characters and hyphens, up to 255), generates one otherwise, exposes it as the
`$*KEAYL-REQUEST-ID` dynamic variable for the rest of the request, and sets the
`X-Request-Id` response header for propagation. The [logger](/docs/MVC-Keayl/logging) prefixes
its line with the id, and `dispatch.keayl` carries it in `request-id`.

```perl6
my $middleware = MVC::Keayl::Middleware::RequestId.new(
  header    => 'X-Request-Id',
  generator => &my-id-generator,
);
```
