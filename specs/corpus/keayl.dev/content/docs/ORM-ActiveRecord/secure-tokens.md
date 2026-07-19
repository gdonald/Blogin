---
title: Tokens & secure data
slug: secure-tokens
order: 31
description: Helpers for passwords, random tokens, and signed ids. Declare them in submethod BUILD. Signed ids and purpose tokens need a signing secret:
toc: true
---
Helpers for passwords, random tokens, and signed ids. Declare them in
`submethod BUILD`. Signed ids and purpose tokens need a signing secret:

```perl6
use ORM::ActiveRecord::Support::Secrets;
secret-key-base('…');          # or set AR_SECRET_KEY_BASE
```

## Passwords

`has-secure-password` adds a virtual `password` attribute (and
`password_confirmation`) whose value is hashed into a `password_digest` column
on save. `authenticate` verifies a candidate.

```perl6
class User is Model {
  submethod BUILD { self.has-secure-password }   # needs a password_digest column
}

my $u = User.create({ email => 'a@x.com', password => 's3cret' });
User.find($u.id).authenticate('s3cret');   # the record, or False
```

Hashing is PBKDF2-HMAC-SHA256 (`:iterations` configurable, default 100000). The
digest is self-describing, so a bcrypt or argon2 hasher can be substituted later
without a migration.

## Secure tokens

`has-secure-token` fills a column with a random url-safe token on create (unless
one was supplied). `regenerate-secure-token` replaces it and saves.
`secure-token` is the standalone generator.

```perl6
class User is Model {
  submethod BUILD { self.has-secure-token('auth_token', :length(24)) }
}

User.secure-token;                  # a url-safe string
$user.regenerate-secure-token('auth_token');
```

## Signed ids

`signed-id` returns a tamper-evident token carrying the record id, scoped to a
`purpose` and optionally expiring. `find-signed` returns the record (or `Nil`);
`find-signed-or-die` throws instead.

```perl6
my $token = $user.signed-id(:purpose('unsubscribe'), :expires-in(86400));
User.find-signed($token, :purpose('unsubscribe'));   # the user, or Nil
```

A wrong purpose, a tampered token, or an expired token all fail to resolve.

## Purpose tokens that self-invalidate

`generates-token-for` ties a token to a value computed from the record. When
that value changes, every previously issued token for that purpose stops
resolving. This is how a password-reset link expires the moment the password
changes.

```perl6
class User is Model {
  submethod BUILD {
    self.generates-token-for('password-reset', :expires-in(900), { .password_digest });
  }
}

my $token = $user.generate-token-for('password-reset');
User.find-by-token-for('password-reset', $token);   # the user, until the digest changes
```
