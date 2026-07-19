---
title: Cookies
slug: cookies
order: 16
description: MVC::Keayl::Cookies is the cookie jar. A controller exposes it as cookies, built from the request's Cookie header and flushed to the response as...
toc: true
---
`MVC::Keayl::Cookies` is the cookie jar. A controller exposes it as `cookies`,
built from the request's `Cookie` header and flushed to the response as
`Set-Cookie` headers after the action runs.

```perl6
self.cookies<theme>;            # read an incoming cookie
self.cookies<theme> = 'dark';   # write a cookie
self.cookies.delete('theme');   # delete a cookie
```

## Attributes

`set` takes the cookie attributes: `path`, `domain`, `expires` (a string or a
`DateTime`, formatted as an HTTP date), `max-age` (seconds), `secure`,
`http-only`, and `same-site`:

```perl6
self.cookies.set('session', $id, path => '/', http-only => True, same-site => 'Lax', secure => True);
```

Assigning a hash sets the value alongside its options:

```perl6
self.cookies<session> = { value => $id, path => '/admin' };
```

`delete` writes a cookie with a past expiry and `Max-Age=0`. Values are
url-encoded on the way out and url-decoded on the way in.

## Signed cookies

`cookies.signed` is a tamper-evident jar. A written value gets an HMAC-SHA1
signature using the controller's `secret`; reading verifies it with a
constant-time comparison and returns the value, or undefined if the signature
does not match (tampering or a different secret):

```perl6
self.cookies.signed<user-id> = $user.id;
my $id = self.cookies.signed<user-id>;   # the value, or Nil if tampered
```

## Encrypted cookies

`cookies.encrypted` keeps the value confidential. It encrypts with AES-256-CBC
under a key derived from the `secret`, then authenticates the ciphertext with an
HMAC (encrypt-then-MAC), so a wrong secret or tampered value fails to decrypt
rather than returning garbage:

```perl6
self.cookies.encrypted<card> = $number;
my $number = self.cookies.encrypted<card>;   # the value, or Nil if it cannot be authenticated
```

The signing and encryption keys come from the controller's `secret`, so set a
strong secret for any application that uses signed or encrypted cookies.
