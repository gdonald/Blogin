---
title: Configuration
slug: configuration
order: 11
description: Two environment variables control how the transport finds Node and how much it reports.
toc: true
---
Two environment variables control how the transport finds Node and how much it
reports.

## `PLAYWRIGHT_NODE`

The path to the Node binary that runs the sidecar.

When unset, the transport searches `PATH` for `node`. When set, it uses that path
directly. If the resolved binary does not exist or is not executable, constructing
a `WWW::Playwright::Sidecar` throws `X::WWW::Playwright::NodeNotFound` with a
message pointing at this variable.

```bash
PLAYWRIGHT_NODE=/opt/node/bin/node raku your-script.raku
```

## `PLAYWRIGHT_DEBUG`

When set to any non-empty value, the transport streams the sidecar's stderr to
the Raku process stderr, each line prefixed with `[playwright-sidecar]`.

```bash
PLAYWRIGHT_DEBUG=1 raku your-script.raku
```

The sidecar's stderr is always captured and available through
`$sidecar.stderr-lines`, regardless of this flag. `PLAYWRIGHT_DEBUG` only controls
whether those lines are also echoed live.
