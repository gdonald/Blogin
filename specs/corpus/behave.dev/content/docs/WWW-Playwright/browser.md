---
title: Browser
slug: browser
order: 7
description: A WWW::Playwright::Browser wraps a launched Chromium instance. Get one from WWW::Playwright.launch.
toc: true
---
A `WWW::Playwright::Browser` wraps a launched Chromium instance. Get one from
`WWW::Playwright.launch`.

## `new-context(--> Context)`

Creates an isolated browser context and returns a
[`Context`](/docs/WWW-Playwright/context). Each context has its own cookies, storage, and pages,
which makes it the right boundary for one test.

```raku
my $context = $browser.new-context;
```

It accepts named options that emulate a device or environment, applied to every
page in the context:

```raku
my $context = $browser.new-context(
  viewport     => { width => 390, height => 844 },
  is-mobile    => True,
  has-touch    => True,
  color-scheme => 'dark',
);
```

| Option                | Effect                                                   |
| --------------------- | -------------------------------------------------------- |
| `viewport`            | `{ width, height }` of the page in pixels.               |
| `user-agent`          | Override the `User-Agent` header.                        |
| `device-scale-factor` | Device pixel ratio.                                      |
| `is-mobile`           | Emulate a mobile device (affects the viewport meta tag). |
| `has-touch`           | Advertise touch support.                                 |
| `reduced-motion`      | `'reduce'` or `'no-preference'`.                         |
| `color-scheme`        | `'light'`, `'dark'`, or `'no-preference'`.               |
| `locale`              | BCP 47 locale, e.g. `'en-US'`.                           |

Only the options you pass are sent; the rest keep Playwright's defaults.

## `close(--> Nil)`

Closes the browser and every context and page under it.

```raku
$browser.close;
```
