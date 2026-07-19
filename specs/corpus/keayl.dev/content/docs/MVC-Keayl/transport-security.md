---
title: Transport & host security
slug: transport-security
order: 21
description: Three middleware harden requests in transit. Add them to the middleware stack.
toc: true
---
Three middleware harden requests in transit. Add them to the middleware stack.

## SSL enforcement

`MVC::Keayl::Middleware::SSL` redirects plain requests to HTTPS and adds an HSTS
header to secure responses. A `GET` or `HEAD` redirect uses `301`; other verbs
use `307` so the method is preserved.

```perl6
MVC::Keayl::Middleware::SSL.new(
  app                => $inner,
  hsts               => True,        # default
  hsts-max-age       => 31536000,
  include-subdomains => True,
  preload            => False,
);
```

The request scheme is taken from `X-Forwarded-Proto` when present, so it works
behind a TLS-terminating proxy.

## Host authorization

`MVC::Keayl::Middleware::HostAuthorization` blocks requests whose `Host` is not on
an allowlist, returning `403`. An entry may be an exact host, a leading-dot
domain that matches its subdomains, or a regex. An empty allowlist permits any
host.

```perl6
MVC::Keayl::Middleware::HostAuthorization.new(
  app     => $inner,
  allowed => ['example.com', '.example.com', /\.internal$/],
);
```

## Secure response headers

`MVC::Keayl::Middleware::SecureHeaders` adds default security headers without
overriding any the application already set: `X-Frame-Options: SAMEORIGIN`,
`X-Content-Type-Options: nosniff`, `X-XSS-Protection: 0`,
`X-Permitted-Cross-Domain-Policies: none`, and
`Referrer-Policy: strict-origin-when-cross-origin`.

`also` adds or overrides headers; set one to `Nil` to drop a default:

```perl6
MVC::Keayl::Middleware::SecureHeaders.new(
  app  => $inner,
  also => %( 'Content-Security-Policy' => "default-src 'self'", 'X-XSS-Protection' => Nil ),
);
```
