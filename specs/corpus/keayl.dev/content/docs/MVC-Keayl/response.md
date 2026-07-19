---
title: Response
slug: response
order: 5
description: MVC::Keayl::Response is a mutable builder for a single outgoing HTTP response. Controllers and middleware set the status, headers, and body on it; the...
toc: true
---
`MVC::Keayl::Response` is a mutable builder for a single outgoing HTTP response.
Controllers and middleware set the status, headers, and body on it; the server
adapter then calls `finish` to obtain the `(status, headers, body)` tuple it
hands back to the connection.

```perl6
use MVC::Keayl::Response;

my $response = MVC::Keayl::Response.new;

$response.status = 201;
$response.content-type('application/json');
$response.location('/users/1');
$response.write('{"id":1}');

my ($status, $headers, $body) = $response.finish;
```

## Construction

| Named argument | Meaning                                   |
| -------------- | ----------------------------------------- |
| `:status`      | HTTP status code. Defaults to `200`.      |
| `:headers`     | A hash of header names to initial values. |
| `:body`        | The initial body content.                 |

## Status

`status` is a writable accessor:

```perl6
$response.status;        # 200
$response.status = 404;
```

## Headers

Header names are matched case-insensitively, and the display casing you set is
preserved on output.

```perl6
$response.set-header('Content-Type', 'text/plain');   # set / replace
$response.add-header('Set-Cookie', 'a=1');            # append another value
$response.add-header('Set-Cookie', 'b=2');

$response.header('content-type');   # 'text/plain'
$response.header('set-cookie');     # 'a=1, b=2'  (values joined for reading)

$response.has-header('Content-Type');   # True
$response.delete-header('Content-Type');

$response.headers;   # { 'Set-Cookie' => 'a=1, b=2' }  (display names, joined values)
```

### Convenience accessors

`content-type` and `location` are getter/setter shortcuts for their headers, and
`content-length` reports the body's UTF-8 byte count:

```perl6
$response.content-type('application/json');
$response.content-type;     # 'application/json'

$response.location('/users/1');
$response.location;         # '/users/1'

$response.content-length;   # byte count of the current body
```

## Body buffering

The body accumulates across `write` calls; the `body` getter returns the joined
buffer, and the `body` setter replaces it:

```perl6
$response.write('Hello, ');
$response.write('world');
$response.body;             # 'Hello, world'

$response.body('replaced'); # discards the buffer
```

## Finalization

`finish` commits the response and returns the adapter tuple: a `(status,
headers, body)` list where `headers` is an itemized list of `name => value`
pairs (one pair per value, so multi-value headers like `Set-Cookie` stay
separate) and `body` is a UTF-8 `Blob`:

```perl6
my ($status, $headers, $body) = $response.finish;

$status;          # 201
$headers.list;    # (Content-Type => ..., Content-Length => ..., ...)
$body;            # Blob
```

On finalization the response supplies a default `Content-Type` of
`text/html; charset=utf-8` when none was set, and always sets `Content-Length`
to the body's byte count.
