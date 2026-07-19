---
title: Overview
slug: overview
order: 1
description: The latest version of this documentation lives at https://docs.keayl.dev/.
toc: true
---
The latest version of this documentation lives at [https://docs.keayl.dev/](https://docs.keayl.dev/).

The homepage for MVC::Keayl is [https://keayl.dev](https://keayl.dev). The source repository is at [https://github.com/gdonald/MVC-Keayl](https://github.com/gdonald/MVC-Keayl).

## Synopsis

MVC::Keayl is a [Model-View-Controller](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93controller)
web framework for Raku.

It is the web layer only. The model layer is delegated to
[ORM::ActiveRecord](https://github.com/gdonald/ORM-ActiveRecord) and default view
rendering to [Template::HAML](https://github.com/gdonald/Template-HAML); both are
pluggable. The HTTP server is reached through an abstract adapter, with the
default adapter built on [Cro](https://cro.raku.org/).

## Pages

- [Request](/docs/MVC-Keayl/request): the incoming HTTP request wrapper.
- [Response](/docs/MVC-Keayl/response): the outgoing HTTP response builder.
- [Middleware](/docs/MVC-Keayl/middleware): the Rack-like middleware stack and endpoint protocol.
- [Server adapters](/docs/MVC-Keayl/adapters): the abstract adapter contract, plus the Cro and in-memory test adapters.
- [Routing](/docs/MVC-Keayl/routing): the routes file DSL and request recognition.
- [Controllers](/docs/MVC-Keayl/controllers): the base controller, per-request state, and action dispatch.
- [Views](/docs/MVC-Keayl/views): template resolution, handlers, and caching.
- [View helpers](/docs/MVC-Keayl/helpers): URL, asset, and tag-building helpers.
- [Cookies](/docs/MVC-Keayl/cookies): the cookie jar with signed and encrypted variants.
- [Sessions](/docs/MVC-Keayl/sessions): the session abstraction with cookie and server-side stores.
- [Flash](/docs/MVC-Keayl/flash): short messages that survive one redirect.
- [CSRF protection](/docs/MVC-Keayl/csrf): authenticity tokens and forgery protection.
- [Parameter filtering](/docs/MVC-Keayl/parameter-filtering): redacting sensitive parameters for logs.
- [Transport & host security](/docs/MVC-Keayl/transport-security): SSL, host authorization, and secure headers.
- [Secrets](/docs/MVC-Keayl/secrets): secret resolution and key derivation.
- [Content negotiation](/docs/MVC-Keayl/content-negotiation): MIME types, `respond-to`, and API controllers.
- [Caching & streaming](/docs/MVC-Keayl/caching): conditional GET, cache headers, fragment caching, and streaming.
- [Application & configuration](/docs/MVC-Keayl/application): the application object, config, boot, and dispatch.
