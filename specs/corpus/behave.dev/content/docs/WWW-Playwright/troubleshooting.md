---
title: Troubleshooting
slug: troubleshooting
order: 12
description: Starting the sidecar throws X::WWW::Playwright::NodeNotFound:
toc: true
---
## Node not found

Starting the sidecar throws `X::WWW::Playwright::NodeNotFound`:

```
Node binary 'node' not found. Install Node, or set PLAYWRIGHT_NODE to its path.
```

The transport could not find `node` on `PATH`. Install Node 18 or newer, or point
`PLAYWRIGHT_NODE` at the binary:

```bash
PLAYWRIGHT_NODE=/opt/node/bin/node raku your-script.raku
```

## npm dependencies missing

Starting the sidecar throws `X::WWW::Playwright::DependenciesMissing`:

```
Sidecar npm dependencies are missing under <dir>. Run bin/www-playwright-setup to install them.
```

The `playwright` npm package has not been installed next to the sidecar. Run
`bin/www-playwright-setup`, which runs `npm install` in the sidecar resources directory.

## Browser not installed

`launch` throws `X::WWW::Playwright::BrowserNotInstalled`:

```
The Chromium browser binary is not installed. Run bin/www-playwright-setup to install it.
```

The npm package is present but the Chromium binary is not. Run `bin/www-playwright-setup`,
which also runs `npx playwright install chromium`.

## Sidecar crash

If the Node process dies mid-run, calls awaiting a response stay unresolved. Set
`PLAYWRIGHT_DEBUG` to stream the sidecar's stderr and see what it reported before
exiting:

```bash
PLAYWRIGHT_DEBUG=1 raku your-script.raku
```

The captured lines are also available on the transport through
`$sidecar.stderr-lines`. See [Configuration](/docs/WWW-Playwright/configuration).
