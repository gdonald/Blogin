---
title: Instrumentation
slug: instrumentation
order: 42
description: ORM::ActiveRecord publishes events as it works: every query, every model load, and every transaction. Subscribe to those events to time queries, trace...
toc: true
---
ORM::ActiveRecord publishes events as it works: every query, every model load,
and every transaction. Subscribe to those events to time queries, trace slow
ones, or feed your own metrics. A built-in log subscriber and a query-log
tagger are provided on top of the same mechanism.

## Notifications

`Notifications` is a small pub/sub. A subscriber registers a callback for a
named event and receives the event's payload (a Hash):

```perl6
use ORM::ActiveRecord::Instrumentation::Notifications;

my $id = Notifications.subscribe('sql.active_record', -> %payload {
  say "{%payload<name>} ran in {(%payload<duration> * 1000).round(0.1)}ms";
});

# later
Notifications.unsubscribe($id);
```

`instrument(event, payload, &block)` runs a block, times it, and notifies
subscribers with the elapsed seconds merged into the payload as `duration`. It
returns the block's value, and still fires the event (with the error in the
payload) when the block throws. `notify(event, payload)` fires an instantaneous
event with no timing.

### Events

| Event                          | Payload keys                                   |
| ------------------------------ | ---------------------------------------------- |
| `sql.active_record`            | `sql`, `binds`, `cached`, `name`, `duration`   |
| `instantiation.active_record`  | `class-name`, `record-count`                   |
| `start_transaction.active_record` | `isolation`                                 |
| `transaction.active_record`    | `outcome` (`commit` / `rollback`), `duration`  |

A `sql.active_record` event fires for every executed query; a query served from
the query cache fires one with `cached => True`. `instantiation.active_record`
fires once per result set loaded into model objects.

## Logging queries with timing

`LogSubscriber` attaches to `sql.active_record` and logs queries with their
timing. By default it logs only slow queries; pass `:log-all` to log every one:

```perl6
use ORM::ActiveRecord::Instrumentation::LogSubscriber;

LogSubscriber.attach(slow-threshold => 0.5);   # log queries at or over 500ms
# ...
LogSubscriber.detach;
```

`slow-threshold` is in seconds. A query whose duration reaches the threshold is
logged as `SLOW (Nms) <sql>` at warning level. Setting `slow_query_threshold` in
a connection block attaches the subscriber automatically.

## Query-log tags

`QueryLogs` appends a SQL comment carrying contextual tags to each query, so a
slow-query log or database tool can trace a statement back to the code that
issued it. A tag value may be a literal or a `Callable` resolved when the
comment is built, which is how request-scoped values are injected:

```perl6
use ORM::ActiveRecord::Instrumentation::QueryLogs;

QueryLogs.enable;
QueryLogs.set-tags([
  application => 'storefront',
  controller  => { $*REQUEST.controller },   # resolved per query
]);

# SELECT * FROM orders /*application:storefront,controller:checkout*/
```

`add-tag(name, value)` appends a single tag, `clear-tags` empties them, and
`disable` turns tagging off. Setting `query_log_tags` in a connection block
enables tagging; a JSON object value is loaded as static tags.

The comment is appended only to the SQL sent to the driver. The query cache
still keys on the untagged SQL, so tags do not cause cache misses. A `*/` in a
tag value is stripped so it cannot terminate the comment early.
