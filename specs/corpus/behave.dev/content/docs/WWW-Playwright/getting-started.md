---
title: Getting started
slug: getting-started
order: 3
description: This walks through launching a browser, opening a page, navigating to a local file, and tearing everything down.
toc: true
---
This walks through launching a browser, opening a page, navigating to a local
file, and tearing everything down.

## Start the driver

`WWW::Playwright.start` spawns the Node sidecar and returns a driver handle.

```raku
use WWW::Playwright;

my $playwright = WWW::Playwright.start;
```

## Launch a browser

`launch` starts Chromium. It is headless by default; pass `:!headless` to see a
window.

```raku
my $browser = $playwright.launch;          # headless
# my $browser = $playwright.launch(:!headless);
```

## Open a context and a page

A context is an isolated browser session, the right boundary for one test. Each
context owns its own pages.

```raku
my $context = $browser.new-context;
my $page    = $context.new-page;
```

## Navigate

`goto` returns the HTTP status of the navigation response.

```raku
my $status = $page.goto('file:///path/to/hello.html');
say $status;   # 200
```

## Tear down

Close from the inside out, then stop the sidecar.

```raku
$page.close;
$context.close;
$browser.close;

$playwright.stop;
```

`stop` closes the sidecar's stdin, which closes any browser the sidecar still
holds before the Node process exits.
