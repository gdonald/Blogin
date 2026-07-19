---
title: Sessions
slug: sessions
order: 17
description: MVC::Keayl::Session is the per-request session. A controller exposes it as session, loaded from a store at the start and persisted after the action runs....
toc: true
---
`MVC::Keayl::Session` is the per-request session. A controller exposes it as
`session`, loaded from a store at the start and persisted after the action runs.
Access is indifferent: keys are coerced to strings, so `session<user-id>` and
`session{'user-id'}` are the same entry.

```perl6
self.session<user-id> = $user.id;   # write
my $id = self.session<user-id>;     # read
self.session<user-id>:delete;       # remove
```

The session tracks whether it was written, so an untouched session persists
nothing.

## reset-session

`reset-session` empties the session. With a cookie store the session cookie is
deleted; with a server-side store the old record is removed and a new id is
issued on the next write. Call it on privilege changes such as login and logout.

```perl6
self.reset-session;
```

## Stores

A controller's `session-store` decides where the data lives. The default is the
cookie store.

### Cookie store

`MVC::Keayl::Session::CookieStore` serializes the session to JSON and keeps it in
a signed cookie (tamper-evident). Set its `serializer` to `encrypted` to keep the
contents confidential as well:

```perl6
MVC::Keayl::Session::CookieStore.new(serializer => 'encrypted');
```

### Server-side store

`MVC::Keayl::Session::ServerSideStore` keeps only a signed session id in the
cookie and stores the data in a pluggable backend. A backend does the
`MVC::Keayl::Session::Backend` role (`read`, `write`, `delete`);
`MVC::Keayl::Session::MemoryBackend` is the in-process implementation:

```perl6
my $store = MVC::Keayl::Session::ServerSideStore.new(
  backend => MVC::Keayl::Session::MemoryBackend.new,
);
```

Implement the `Backend` role over a database or cache to share sessions across
processes. Signing and the session secret come from the controller's `secret`.
