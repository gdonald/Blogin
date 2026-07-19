---
title: Installation
slug: installation
order: 2
description: This distribution is glue. It pulls in the two pieces it sits between, but the browser itself comes from WWW::Playwright, which needs Node and a Chromium...
toc: true
---
This distribution is glue. It pulls in the two pieces it sits between, but the
browser itself comes from WWW::Playwright, which needs Node and a Chromium binary.

## The three pieces

- **BDD::Behave** — the `describe`/`context`/`it`/`expect` DSL and the matcher
  protocol. Pure Raku.
- **WWW::Playwright** — drives a real browser through a long-lived Node sidecar.
  Needs Node, the `playwright` npm package, and a Chromium binary.
- **BDD::Behave::Playwright** — this distribution. Lifecycle hooks and matchers
  that connect the two. It does no browser driving of its own.

## Install

Installing this distribution brings in both Raku dependencies:

```
zef install BDD::Behave::Playwright
```

## Install the browser runtime

WWW::Playwright's sidecar needs its npm dependencies and a Chromium binary. After
installing, run its installer once:

```
install
```

That runs `npm install` for the sidecar and `npx playwright install chromium`.
You also need Node available on `PATH` (override with `PLAYWRIGHT_NODE`).

## Verify

A one-example spec confirms the whole stack resolves and a browser launches:

```raku
use BDD::Behave;
use BDD::Behave::Playwright;

describe 'smoke', {
  playwright-page(fixture => 'specs/fixtures/hello.html');

  it 'loads the page', {
    expect(page.locator('#greeting')).to.be-visible;
  }
}
```
