---
title: Background jobs
slug: jobs
order: 28
description: MVC::Keayl::Job runs work either now or later. A job subclass implements perform, and the framework runs it synchronously or hands it to a queue adapter...
toc: true
---
`MVC::Keayl::Job` runs work either now or later. A job subclass implements
`perform`, and the framework runs it synchronously or hands it to a queue adapter
for later execution.

## Defining a job

```perl6
class WelcomeJob is MVC::Keayl::Job {
  method perform($user-id) {
    # send a welcome email, warm a cache, ...
  }
}
```

- `WelcomeJob.perform-now($id)` runs `perform` immediately and returns its result.
- `WelcomeJob.perform-later($id)` enqueues the job on the configured adapter and
  returns the job. Positional and named arguments both pass straight through to
  `perform`.

Every job is on the `default` queue unless changed.

## Queue adapters

The adapter decides what "later" means. Set it once, for all jobs:

```perl6
MVC::Keayl::Job.queue-adapter(MVC::Keayl::Job::QueueAdapter::Async.new);
```

A queue adapter is any `MVC::Keayl::Job::QueueAdapter` (a role with one
`enqueue($job)` method). Three are built in:

- **Inline** (`...::QueueAdapter::Inline`) runs the job on enqueue, so
  `perform-later` behaves like `perform-now`.
- **Test** (`...::QueueAdapter::Test`) collects enqueued jobs in `enqueued`
  without running them. `perform-all` runs and drains them, `clear` empties the
  queue.
- **Async** (`...::QueueAdapter::Async`) runs each job on its own thread and
  records the promises; `wait` blocks until the enqueued jobs finish.

With no adapter configured, `perform-later` runs the job inline, so a job is
always runnable without setup.

```perl6
my $adapter = MVC::Keayl::Job::QueueAdapter::Test.new;
MVC::Keayl::Job.queue-adapter($adapter);

WelcomeJob.perform-later(42);   # collected, not run
$adapter.perform-all;           # now it runs
```

`reset-queue-adapter` clears the configured adapter, returning to the inline
default.

### Durable adapter

`...::QueueAdapter::Database` persists each job through a pluggable store instead
of holding it in memory. `enqueue` serializes the job (class, arguments, queue,
priority, scheduled time) into the store; `work` reads the eligible rows, runs
them, and removes each as it finishes. Jobs run highest priority first, and a job
scheduled for the future is skipped until its time arrives (judged against the
adapter's `clock`):

```perl6
my $adapter = MVC::Keayl::Job::QueueAdapter::Database.new;
MVC::Keayl::Job.queue-adapter($adapter);

WelcomeJob.perform-later(42);   # persisted in the store
$adapter.work;                  # runs every due job
```

`MemoryStore` is the built-in store; a DB-backed store implements the same
`Store` role (`insert`, `eligible`, `remove`, `all`).

## Scheduling

`set` configures a job before enqueuing it:

```perl6
WelcomeJob.set(queue => 'mailers', priority => 10).perform-later(42);
WelcomeJob.set(wait => 300).perform-later(42);         # 300 seconds from now
WelcomeJob.set(wait-until => $epoch).perform-later(42); # at an absolute time
```

`wait` is relative to the job clock (`MVC::Keayl::Job.clock`, settable for tests);
`wait-until` is an absolute time. The scheduled time is recorded on the job and
honored by the durable adapter.

## Retries, discards, and rescues

`retry-on` re-enqueues a job when a matching error is raised, up to `attempts`
total runs, waiting `wait` between tries. `wait` is a number of seconds or a
callable of the attempt number for a backoff. When the attempts run out, the
job runs the `with` block or re-raises:

```perl6
class ImportJob is MVC::Keayl::Job { ... }
ImportJob.retry-on(X::Timeout, wait => 5, attempts => 3);
ImportJob.retry-on(X::RateLimited, wait => -> $n { $n * $n }, attempts => 10);
```

`discard-on` swallows a matching error so the job is dropped without retrying.
`rescue-from` runs a handler for a matching error:

```perl6
ImportJob.discard-on(X::RecordGone);
ImportJob.rescue-from(X::Parse, -> $job, $error { report($error) });
```

The most specific handler for the raised error wins, and a later registration
beats an earlier one at the same specificity.

## Callbacks

`before-perform`, `after-perform`, and `around-perform` wrap a job's execution;
`before-enqueue`, `after-enqueue`, and `around-enqueue` wrap enqueuing. Each
before/after callback takes the job; each around callback takes the job and the
continuation:

```perl6
ImportJob.before-perform(-> $job { ... });
ImportJob.around-perform(-> $job, &next { instrument({ next() }) });
```

## Arguments and Global IDs

A job argument that is a record (an object with an `id` whose class has a `find`)
serializes to a Global ID string, `gid://keayl/<Class>/<id>`, and is relocated
when the job runs. This lets the durable adapter persist a reference to a record
rather than a copy:

```perl6
use MVC::Keayl::Job::GlobalID;

to-gid($user);                       # 'gid://keayl/User/42'
locate('gid://keayl/User/42');       # User.find(42)
```

`serialize-arguments` and `deserialize-arguments` apply this over a whole
argument capture, leaving plain values untouched and converting records to and
from Global IDs.
