---
title: Overview
slug: overview
order: 1
description: Thin glue between BDD::Behave and WWW::Playwright. It adds browser lifecycle hooks and Playwright-aware matchers to behave examples.
toc: true
---
Thin glue between [BDD::Behave](https://github.com/gdonald/BDD-Behave) and
[WWW::Playwright](https://github.com/gdonald/WWW-Playwright). It adds browser
lifecycle hooks and Playwright-aware matchers to behave examples.

It contains no browser driving of its own. All of that lives in WWW::Playwright.
This distribution is the layer that wires a Playwright page into a behave
`describe`/`it` flow and lets you write expectations against it.

## Dependency split

```
BDD::Behave::Playwright  ->  BDD::Behave
        |
        +----------------->  WWW::Playwright
```

- `BDD::Behave` provides the `describe`/`context`/`it`/`expect` DSL and the
  matcher protocol.
- `WWW::Playwright` drives a real browser through a long-lived Node sidecar.
- `BDD::Behave::Playwright` (this distribution) connects the two: it manages the
  browser lifecycle around each example and exposes matchers that assert on
  Playwright `Page` and `Locator` objects.

## What the glue adds

- Lifecycle hooks that launch a browser context and page for each example and
  close them afterward, so examples stay isolated.
- The current `page` exposed to examples as a named subject.
- Matchers that reuse Playwright's auto-waiting instead of re-implementing waits.
- Failure diagnostics that capture a screenshot when an example fails.

## Installation

```
zef install BDD::Behave::Playwright
```

The browser itself is provided by WWW::Playwright, which needs Node and a
Playwright Chromium binary available at runtime.
