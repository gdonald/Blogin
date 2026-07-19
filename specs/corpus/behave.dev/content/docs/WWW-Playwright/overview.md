---
title: Overview
slug: overview
order: 1
description: A Raku driver for Playwright. Raku owns the API surface and lifecycle. Node owns the browser.
toc: true
---
A Raku driver for [Playwright](https://playwright.dev). Raku owns the API surface
and lifecycle. Node owns the browser.

## Architecture

Raku spawns a long-lived Node process, the sidecar, that imports the official
`playwright` npm package. Raku and the sidecar talk over newline-delimited
JSON-RPC on stdio.

```
+-----------+        NDJSON / stdio        +--------------+        +---------+
|   Raku    | ---------------------------> | Node sidecar | -----> | Browser |
| (driver)  | <--------------------------- | (playwright) | <----- |         |
+-----------+        JSON-RPC 2.0          +--------------+        +---------+
```

- Raku sends one JSON-RPC request per line and reads one response per line.
- The sidecar dispatches each request to Playwright and replies on the same
  line-buffered stream.
- Browser objects (pages, locators, element handles, contexts) never cross the
  wire. The sidecar keeps them in a registry and mints opaque string handles
  (`page@1`, `loc@7`) that Raku passes back in later requests.

## Why a sidecar

Playwright ships as a Node library with no stable cross-language RPC surface.
Rather than reimplement the protocol, this dist drives the real Node package and
keeps Raku as the orchestration layer. Raku starts the sidecar, feeds it
requests, correlates responses, and tears it down on exit.

## Layout

- `WWW::Playwright` - entry point: start the sidecar, launch a browser, stop.
- `WWW::Playwright::Sidecar` - the transport that owns the Node process.
- `WWW::Playwright::Browser`, `::Context`, `::Page`, `::Locator` - thin Raku
  wrappers around sidecar handles.
- `WWW::Playwright::Exception` - typed exceptions built from JSON-RPC errors.
