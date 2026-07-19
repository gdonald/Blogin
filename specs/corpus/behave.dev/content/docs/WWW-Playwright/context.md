---
title: Context
slug: context
order: 8
description: A WWW::Playwright::Context is an isolated browser session. Get one from Browser.new-context.
toc: true
---
A `WWW::Playwright::Context` is an isolated browser session. Get one from
[`Browser.new-context`](/docs/WWW-Playwright/browser).

## `new-page(--> Page)`

Opens a new page in the context and returns a [`Page`](/docs/WWW-Playwright/page).

```raku
my $page = $context.new-page;
```

## `start-tracing(--> Nil)`

Starts recording a trace (screenshots and DOM snapshots) for the context.

```raku
$context.start-tracing;
```

## `stop-tracing(Str :$path --> Nil)`

Stops tracing. With `:path`, writes the trace zip to that path.

```raku
$context.stop-tracing(path => '/tmp/trace.zip');
```

See [Diagnostics](/docs/WWW-Playwright/diagnostics) for the full tracing workflow.

## `close(--> Nil)`

Closes the context and its pages.

```raku
$context.close;
```
