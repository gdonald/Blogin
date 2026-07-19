---
title: Logging
slug: logging
order: 41
description: ORM::ActiveRecord uses Log::Async. Loading the library configures the INFO-level handler to write to $*OUT (stdout). That is the only log destination set...
toc: true
---
ORM::ActiveRecord uses [Log::Async](https://raku.land/zef:tony-o/Log::Async).
Loading the library configures the `INFO`-level handler to write to `$*OUT`
(stdout). That is the only log destination set up by default — loading the
library does not touch the filesystem.

## File logging

Set `ORM_LOG_FILE` in the process environment to an absolute or relative path,
and `ERROR`-level messages will be appended to that file:

```shell
export ORM_LOG_FILE=log/error.log
```

```perl6
use ORM::ActiveRecord::Model;   # adapters log via Support::Log
# ERROR-level entries now also write to ./log/error.log
```

Notes:

- The path is resolved against the **current working directory** at the time
  the library is loaded. If the directory doesn't exist, opening the file
  will fail — create it first (`mkdir log`) or use an absolute path.
- The variable is read once, when `ORM::ActiveRecord::Support::Log` is first
  loaded. Setting it after that point has no effect on the current process.
- Unset or empty `ORM_LOG_FILE` ⇒ no file handler is attached. This keeps
  the library safe to load under tooling that runs from arbitrary working
  directories (LSP precompilation, CI matrices, cron jobs).
- The `bin/active-record` migration CLI defaults `ORM_LOG_FILE` to `log/error.log` —
  but **only when a `log/` directory exists in the current working
  directory**. So running `active-record` from the project root keeps the old
  file-logging behavior, while precompilation under tooling that runs from
  elsewhere (LSP, CI checkout from a parent dir, etc.) skips it cleanly.
  Export a different value before invoking `active-record` to override.

## Structured configuration

`Support::Log` has a small configuration layer over the SQL log: a minimum
level, a text-or-JSON formatter, an ANSI colour toggle, and a pluggable sink.
The defaults reproduce the original colourised text routed through Log::Async.

```perl6
use ORM::ActiveRecord::Support::Log;

Log.set-level('warn');     # only warn / error entries emit (slow queries are warn)
Log.set-format('json');    # one JSON object per line instead of coloured text
Log.set-colour(False);     # plain text (e.g. when writing to a file)
Log.set-sink(-> $line, $level { $*ERR.say($line) });   # route lines anywhere
Log.reset;                 # back to the defaults
```

`configure` sets several at once and is what the config file drives:

```perl6
Log.configure(level => 'info', format => 'json', colour => False);
```

In `config/application.json`, a `logging` object on a connection applies the
same settings at boot:

```json
{
  "development": {
    "primary": {
      "adapter": "postgres",
      "name": "myapp_dev",
      "logging": { "level": "info", "format": "text", "color": true },
      "log_queries": true,
      "slow_query_threshold": 0.5
    }
  }
}
```

The JSON formatter emits one object per line, which a structured-log collector
can ingest directly:

```json
{"kind":"query","level":"info","sql":"SELECT * FROM users WHERE id = ?","ms":3,"slow":false,"binds":["7"]}
```

## SQL log with timing and bound values

Slow queries (at or over `slow_query_threshold`) are always logged with their
duration. Set `log_queries: true` (or call `LogSubscriber.attach(:log-all)`) to
log **every** query, each with its timing and the bound parameter values:

```
(3ms) SELECT * FROM users WHERE id = ? [binds: 7]
SLOW (812ms) UPDATE orders SET state = ? WHERE id = ? [binds: 'shipped', 42]
```

Bound values render with numbers bare, strings quoted, and `NULL` for an
undefined value. See [Instrumentation](/docs/ORM-ActiveRecord/instrumentation) for the underlying
`sql.active_record` event and the `LogSubscriber` that formats these lines.

## Disabling SQL logging

Adapters emit each query at `INFO` level. To silence them, set:

```shell
export DISABLE-SQL-LOG=1
```

This is typically what you want under test suites — `t/` already sets it
where appropriate.
