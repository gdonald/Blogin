---
title: Error reporting
slug: error-reporting
order: 35
description: An unhandled error from a controller action becomes a 500. What the response shows depends on the environment, and every such error is offered to the...
toc: true
---
An unhandled error from a controller action becomes a `500`. What the response
shows depends on the environment, and every such error is offered to the
registered error reporters.

## The developer page

In development (`verbose-errors`), a `500` renders an HTML page built by
`MVC::Keayl::ExceptionPage` showing:

- the exception class and message,
- the backtrace,
- the request (method, path, controller, action, request id),
- the parameters, with the [parameter filter](/docs/MVC-Keayl/parameter-filtering) applied,
- the application's route table.

Every dynamic value is HTML-escaped, so an exception message or parameter value
containing markup is shown as text rather than rendered.

In production the same `500` is a terse `Internal Server Error` with no details.

## Reporters

`MVC::Keayl::ErrorReporter` is a role with one method:

```perl6
method report(Exception:D $error, %context) { ... }
```

`%context` carries the `request`, `method`, `path`, `controller`, `action`,
filtered `params`, and `request-id`. Register a reporter on the application:

```perl6
class SentryReporter does MVC::Keayl::ErrorReporter {
  method report(Exception:D $error, %context) {
    # ship $error and %context to your error tracker
  }
}

$app.report-errors-with(SentryReporter.new);
```

Reporters run on every `500`, in registration order, before the response is
built. A reporter that itself throws is isolated, so one failing reporter never
masks the original error or stops the others. With no reporters registered,
reporting is a no-op.

`MVC::Keayl::ErrorReporting` is the registry behind this: `subscribe` adds a
reporter and `report` fans an error out to all of them. The application builds one
from the reporters you register and hands it to the dispatcher.
