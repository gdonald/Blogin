---
title: Wire contract
slug: wire-contract
order: 13
description: Raku and the Node sidecar speak JSON-RPC 2.0 over stdio with NDJSON framing. Each message is a single line of compact JSON terminated by \n. The wire is...
toc: true
---
Raku and the Node sidecar speak JSON-RPC 2.0 over stdio with NDJSON framing.
Each message is a single line of compact JSON terminated by `\n`. The wire is
never pretty-printed; a raw newline inside a message would break framing.

## Messages

Request:

```json
{"jsonrpc":"2.0","id":1,"method":"ping","params":{}}
```

Success response:

```json
{"jsonrpc":"2.0","id":1,"result":"pong"}
```

Failure response:

```json
{"jsonrpc":"2.0","id":1,"error":{"code":-32000,"message":"...","data":{"name":"Error","stack":"..."}}}
```

Every request carries a unique `id`. The sidecar echoes that `id` on the
matching response so Raku can correlate replies to in-flight calls. Responses
may arrive in any order; correlation is by `id`, not arrival order.

## Error codes

The sidecar follows the JSON-RPC 2.0 reserved range and adds one
implementation-defined code for Playwright failures.

| Code     | Meaning                                              |
|----------|------------------------------------------------------|
| `-32700` | Parse error: the line was not valid JSON.            |
| `-32600` | Invalid request: missing or non-string `method`.     |
| `-32601` | Method not found.                                    |
| `-32000` | A Playwright or handler error was thrown.            |

A failure response carries the original error `name` and `stack` in `data` so
the Raku side can rebuild a typed exception.

## Handle model

Browser objects never cross the wire. The sidecar keeps a registry mapping
opaque string handles to live objects (a `Browser`, `BrowserContext`, `Page`,
`Locator`, or `ElementHandle`). Handles are minted as `kind@N`:

```
browser@1
context@1
page@1
loc@7
```

A method that creates an object returns its handle. A later request passes the
handle back in `params.handle` (or another named field) and the sidecar resolves
it to the live object before acting. Resolving an unknown handle is an error.

## Methods

| Method   | Params                  | Result                  |
|----------|-------------------------|-------------------------|
| `ping`   | none                    | `"pong"`                |
| `launch` | `{ headless: Bool }`    | a `browser@N` handle    |
| `close`  | `{ handle: "kind@N" }`  | `null`                  |

`launch` starts Chromium. `close` closes the object behind a handle and forgets
it. When the sidecar's stdin closes, it closes every browser it launched before
exiting.
