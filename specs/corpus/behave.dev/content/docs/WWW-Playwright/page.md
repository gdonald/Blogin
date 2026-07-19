---
title: Page
slug: page
order: 9
description: A WWW::Playwright::Page is a single tab. Get one from Context.new-page.
toc: true
---
A `WWW::Playwright::Page` is a single tab. Get one from
[`Context.new-page`](/docs/WWW-Playwright/context).

## `goto(Str $url --> Int)`

Navigates to a URL and returns the HTTP status of the navigation response.

```raku
my $status = $page.goto('file:///path/to/hello.html');   # 200
```

## `url(--> Str)`

The current page URL.

```raku
my $url = $page.url;   # 'file:///path/to/hello.html'
```

## `title(--> Str)`

The document title.

```raku
my $title = $page.title;   # 'Hello'
```

## `locator(Str $selector --> Locator)`

Returns a [`Locator`](/docs/WWW-Playwright/locator) for the selector. This is the entry point to
every action and query.

```raku
my $heading = $page.locator('#greeting');
```

## `screenshot(Str :$path --> Buf)`

Captures the page as a PNG and returns the bytes. With `:path`, also writes the
image to that path.

```raku
my $bytes = $page.screenshot(path => '/tmp/page.png');
```

See [Diagnostics](/docs/WWW-Playwright/diagnostics).

## `close(--> Nil)`

Closes the page.

```raku
$page.close;
```
