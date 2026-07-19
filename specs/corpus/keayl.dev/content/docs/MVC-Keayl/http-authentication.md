---
title: HTTP authentication
slug: http-authentication
order: 22
description: MVC::Keayl::Controller provides the three ActionController::HttpAuthentication schemes as controller helpers: Basic, Token, and Digest. The parsing,...
toc: true
---
`MVC::Keayl::Controller` provides the three `ActionController::HttpAuthentication`
schemes as controller helpers: Basic, Token, and Digest. The parsing,
encoding, nonce, and constant-time comparison primitives live in
`MVC::Keayl::HttpAuthentication`.

Every credential comparison uses `secure-compare`, a constant-time check that
returns false for unequal lengths and never short-circuits on the first
differing byte.

## Basic

`authenticate-with-http-basic` decodes the `Authorization: Basic` header and
calls the block with the username and password, returning the block's value, or
`Nil` when the header is absent or not Basic:

```perl6
method show {
  my $user = self.authenticate-with-http-basic(-> $name, $password {
    User.authenticate($name, $password)
  });
}
```

`request-http-basic-authentication` issues a `401` with a
`WWW-Authenticate: Basic realm="..."` challenge and returns false.
`authenticate-or-request-with-http-basic` combines the two: it runs the block
and, when it returns a falsy value, issues the challenge:

```perl6
method show {
  if self.authenticate-or-request-with-http-basic('Admin', &check) {
    self.render(plain => 'welcome');
  }
}
```

`http-basic-authenticate-with` is a class-level shortcut that registers a
before-action authenticating one fixed credential pair, honouring `only` and
`except`:

```perl6
DashboardController.http-basic-authenticate-with(
  name => 'admin', password => 'secret', realm => 'Admin', except => <index>,
);
```

It also has an `is http-basic-authenticate-with` trait form for the class header:

```perl6
class DashboardController is MVC::Keayl::Controller
  is http-basic-authenticate-with(name => 'admin', password => 'secret', except => <index>) { }
```

## Token

`authenticate-with-http-token` parses an `Authorization: Token` or
`Authorization: Bearer` header into the token and any additional `key="value"`
options, then calls the block:

```perl6
method show {
  my $ok = self.authenticate-with-http-token(-> $token, %options {
    ApiKey.valid($token)
  });
}
```

Both `Token abc`, `Bearer abc`, and `Token token="abc", nonce="42"` forms are
recognized. `request-http-token-authentication` issues the
`WWW-Authenticate: Token realm="..."` challenge, and
`authenticate-or-request-with-http-token` combines the two.

## Digest

`authenticate-or-request-with-http-digest` validates an
`Authorization: Digest` response. It checks the nonce, looks the password up
through the block, computes the expected response, and compares it in constant
time. The block returns the user's password (or the precomputed `HA1`):

```perl6
method show {
  my $user = self.authenticate-or-request-with-http-digest('Admin', -> $username {
    User.find-by(:$username).?password
  });

  self.render(plain => "hello $user") if $user;
}
```

Nonces are signed with the controller's secret and carry their creation time,
so `request-http-digest-authentication` issues a fresh
`WWW-Authenticate: Digest realm="...", qop="auth", nonce="...", opaque="..."`
challenge and `validate-digest-nonce` rejects forged or expired nonces (the
default lifetime is 300 seconds).
