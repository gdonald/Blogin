---
title: Failure diagnostics
slug: diagnostics
order: 6
description: When an example managed by playwright-page fails, the lifecycle captures a screenshot of the page before the context is closed, so you can see what the...
toc: true
---
When an example managed by `playwright-page` fails, the lifecycle captures a
screenshot of the page before the context is closed, so you can see what the
browser looked like at the point of failure. The capture never throws, so a
diagnostic problem cannot turn a passing example into a failing one.

## Artifacts directory

Screenshots are written to an artifacts directory, `playwright-artifacts` by
default. Override it per `describe` or for the whole run:

```raku
describe 'checkout', {
  playwright-page(fixture => 'specs/fixtures/checkout.html', artifacts => 'tmp/shots');
  # ...
}
```

The environment variable `PLAYWRIGHT_ARTIFACTS` sets the default for every
example. An explicit `artifacts => ...` argument takes precedence.

Each failure writes `failure-<pid>-<n>.png`, unique per worker process and
example, so parallel workers do not collide.

## Traces

A Playwright trace can be dumped alongside the screenshot, gated so it stays off
by default. Enable it with `trace => True` or the `PLAYWRIGHT_TRACE` environment
variable:

```raku
playwright-page(fixture => 'specs/fixtures/checkout.html', trace => True);
```

When tracing is on, each example records a trace; on failure the trace is written
as `failure-<pid>-<n>.zip` next to the screenshot, and on success it is discarded.
Open a trace with `npx playwright show-trace <file>.zip`.
