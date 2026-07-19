---
title: CSRF protection
slug: csrf
order: 19
description: Cross-site request forgery protection checks that an unsafe request carries a token that was issued to the current session. Enable it on a controller with...
toc: true
---
Cross-site request forgery protection checks that an unsafe request carries a
token that was issued to the current session. Enable it on a controller with
`protect-from-forgery`:

```perl6
class ApplicationController is MVC::Keayl::Controller { }
ApplicationController.protect-from-forgery;
```

It also has an `is protect-from-forgery` trait form for the class header:

```perl6
class ApplicationController is MVC::Keayl::Controller is protect-from-forgery { }
```

This registers a before-action that verifies the token on every unsafe verb
(`POST`, `PUT`, `PATCH`, `DELETE`). Safe verbs (`GET`, `HEAD`, `OPTIONS`,
`TRACE`) are not checked.

## The token

`csrf-token` returns a token for a form. The session holds one real token; each
call masks it with a fresh one-time pad, so the value differs every render while
still validating against the same real token. Put it in a form with the
`csrf-token` option:

```perl6
form-with(model => $post, url => '/posts', csrf-token => self.csrf-token, content => -> $f {
  $f.text-field('title')
});
```

A request may also supply the token in the `X-CSRF-Token` header, which suits
JavaScript clients. Verification accepts a masked or an unmasked token and
compares in constant time.

## Strategies

`protect-from-forgery` takes a `with` strategy for a failed check:

```perl6
ApplicationController.protect-from-forgery(with => 'exception');     # default
ApplicationController.protect-from-forgery(with => 'reset-session');
ApplicationController.protect-from-forgery(with => 'null-session');
```

- `exception` raises `X::MVC::Keayl::InvalidAuthenticityToken`, which the base
  controller turns into a `422` response.
- `reset-session` and `null-session` clear the session and let the request
  proceed without the prior identity.

## Skipping protection

API and webhook endpoints that authenticate another way can opt out with
`skip-forgery-protection`, using the same `only`/`except` filters as the
callbacks:

```perl6
ApiController.skip-forgery-protection(only => ['create', 'update']);
```
