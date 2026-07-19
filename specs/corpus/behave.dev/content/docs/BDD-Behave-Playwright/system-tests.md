---
title: System tests
slug: system-tests
order: 4
description: playwright-page wires the browser into one describe block at a time. When most of a suite's browser specs share the same setup, you can register the...
toc: true
---
`playwright-page` wires the browser into one `describe` block at a time. When most
of a suite's browser specs share the same setup, you can register the lifecycle
once in a `.behave` config file and tag the relevant groups instead of calling
`playwright-page` in each one.

## Register the lifecycle once

`BDD::Behave::Playwright.install($config)` registers the browser lifecycle as
config-level hooks and exposes the current page as a helper. Put it in your
project's `.behave` file:

```raku
use BDD::Behave::Playwright;

configure-behave -> $config {
  BDD::Behave::Playwright.install($config);
}
```

By default the lifecycle applies to every group tagged `type => system`. The
browser launches in a `before-all`, a fresh context and page open in a
`before-each`, and they tear down (writing failure artifacts when an example
failed) in `after-each` / `after-all`.

## Tag the group

Mark a `describe` with `:type<system>`. Every example inside it gets a live
browser page, reachable as the `page` term:

```raku
use BDD::Behave;
use BDD::Behave::Playwright;

describe 'the greeting page', :type<system>, {
  it 'shows the greeting', {
    page.goto(fixture-url('specs/fixtures/hello.html'));
    expect(page.locator('#greeting')).to.have-text('Hello, world');
  }
}
```

`use BDD::Behave::Playwright` brings the `page` term (and `fixture-url`) into the
spec. The same page is also reachable as `.page` on the topic in a `-> $_ { ... }`
block, which needs no import. Unlike `playwright-page`, the config path does not
navigate for you, so call `page.goto(...)` (with `fixture-url` for local files, or
a real URL) at the start of the example.

## Choosing the tag

Pass `:type` to use a different tag, for example to separate full-page system
tests from feature tests:

```raku
configure-behave -> $config {
  BDD::Behave::Playwright.install($config, :type<feature>);
}
```

```raku
describe 'checkout', :type<feature>, {
  it 'reaches the confirmation page', { ... }
}
```

## Artifacts and tracing

`install` reads the same settings as `playwright-page`: pass `:artifacts` to set
the output directory (or use the `PLAYWRIGHT_ARTIFACTS` environment variable), and
`:trace` to record a Playwright trace (or set `PLAYWRIGHT_TRACE`). Screenshots and
traces are written only for examples that fail.

```raku
BDD::Behave::Playwright.install($config, :artifacts<tmp/playwright>, :trace);
```

## When to use which

- `playwright-page` suits a handful of browser specs, or groups that each load a
  different fixture, where keeping the setup beside the examples reads clearly.
- `install` plus `:type<system>` suits a suite with many browser specs that share
  one setup, keeping the lifecycle in one place.
