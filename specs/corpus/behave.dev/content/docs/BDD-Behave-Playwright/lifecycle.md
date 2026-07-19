---
title: Browser lifecycle
slug: lifecycle
order: 3
description: playwright-page wires the browser lifecycle into a describe block. It launches one browser for the group, creates a fresh context and page for each...
toc: true
---
`playwright-page` wires the browser lifecycle into a `describe` block. It launches
one browser for the group, creates a fresh context and page for each example, and
closes them afterward so examples stay isolated.

## Setup

Call `playwright-page` at the top of a `describe`, passing the fixture to load:

```raku
use BDD::Behave;
use BDD::Behave::Playwright;

describe 'the greeting page', {
  playwright-page(fixture => 'specs/fixtures/hello.html');

  it 'shows the greeting', {
    expect(page.locator('#greeting').text-content).to.eq('Hello, world');
  }
}
```

## The `page` term

`use BDD::Behave::Playwright` brings a `page` term into scope that returns the
current page:

```raku
it 'fills a field', {
  page.locator('#name').fill('Ada');
  expect(page.locator('#name').input-value).to.eq('Ada');
}
```

The same page is also reachable through the example's topic parameter as `.page`,
for blocks written `-> $_ { ... }`:

```raku
it 'fills a field', -> $_ {
  .page.locator('#name').fill('Ada');
}
```

Each example gets its own context and page, so state set in one example does not
leak into the next.

## Scope

- One browser is launched per `describe` group, in a `before-all` hook.
- Each example gets a fresh context and page, created in `before-each` and
  published as the `page` term.
- The context is closed in `after-each`; the browser is closed in `after-all`.

## Watching the browser

The browser runs headless by default. Set `SHOW_CHROME` to launch it headed, so
the window is visible while the examples drive it:

```sh
SHOW_CHROME=1 behave specs/some-browser-spec.raku
```

`headless-from-env` reads the variable and is what `playwright-page` passes to
`launch`. An unset variable, or the value `0`, stays headless; any other value
runs headed. Headed runs suit watching a suite locally. Leave it unset in CI.

## Fixtures

`fixture-url` turns a filesystem path into an absolute `file://` URL:

```raku
fixture-url('specs/fixtures/hello.html');  # file:///abs/path/specs/fixtures/hello.html
```

`playwright-page(fixture => ...)` calls it for you. Pass an absolute or
working-directory-relative path to a local HTML file. Browser tests run against
local `file://` fixtures, never the network.

## A base URL and `visit`

To drive a running application instead of a file fixture, give `playwright-page`
a `base-url`. `visit` then navigates the current page to a path beneath it:

```raku
describe 'the dashboard', {
  playwright-page(base-url => 'http://127.0.0.1:8080');

  it 'greets the user', -> $_ {
    visit('/dashboard');
    expect(.page.locator('#welcome')).to.have-text('Welcome');
  }
}
```

`visit('/path')` joins the path onto the base URL. An absolute URL
(`visit('http://...')`) is used unchanged. `goto($url)` navigates to a URL
directly, without the base.

The base URL may be a callable, which is resolved at navigation time. That suits
a server whose address is known only once it has started, such as a test server
bound to an ephemeral port:

```raku
playwright-page(base-url => { test-server-url() });
```

## Context options

`context` passes options through to the browser context that each example runs
in, so a `describe` can emulate a device, such as a mobile viewport, a color
scheme, or a locale:

```raku
describe 'the mobile menu', {
  playwright-page(
    base-url => 'http://127.0.0.1:8080',
    context  => { viewport => { width => 390, height => 844 } });

  it 'shows the hamburger toggle', -> $_ {
    visit('/');
    expect(.page.locator('.menu-toggle')).to.be-visible;
  }
}
```

The keys are the options that [WWW::Playwright's
`new-context`](https://github.com/gdonald/WWW-Playwright) accepts, such as
`viewport`, `is-mobile`, `has-touch`, `color-scheme`, and `locale`.
