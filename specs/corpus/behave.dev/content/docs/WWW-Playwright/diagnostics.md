---
title: Diagnostics
slug: diagnostics
order: 6
description: Two tools for capturing what happened during a run: screenshots and traces.
toc: true
---
Two tools for capturing what happened during a run: screenshots and traces.

## Screenshots

`Page.screenshot` returns the image bytes as a `Buf`. Pass `:path` to also write
the image to disk.

```raku
# bytes only
my $bytes = $page.screenshot;

# write a PNG and get the bytes back
my $bytes = $page.screenshot(path => '/tmp/page.png');
```

The bytes are a PNG, so they begin with the PNG signature
(`137, 80, 78, 71`).

## Tracing

Tracing records screenshots and DOM snapshots for the lifetime of a context,
then writes them to a zip that the Playwright trace viewer can open. This is what
the BDD::Behave glue uses to dump a trace when an example fails.

Start tracing on the context before driving it, then stop and write the zip.

```raku
my $context = $browser.new-context;
$context.start-tracing;

my $page = $context.new-page;
$page.goto('file:///path/to/hello.html');
$page.locator('#go').click;

$context.stop-tracing(path => '/tmp/trace.zip');
```

Open the result with the Playwright CLI:

```bash
npx playwright show-trace /tmp/trace.zip
```
