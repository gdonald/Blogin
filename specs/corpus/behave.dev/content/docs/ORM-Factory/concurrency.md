---
title: Concurrency
slug: concurrency
order: 25
description: ORM::Factory's registry, sequences, and build pipeline are designed for concurrent use. The guarantees here are explicit; assumptions outside this page...
toc: true
---
`ORM::Factory`'s registry, sequences, and build pipeline are designed for
concurrent use. The guarantees here are explicit; assumptions outside this
page are not promises.

## What is thread-safe

- **Reads** of the factory registry (`factory-by-name`, `factories`,
  `aliases`, `factory-names`, `variant-names-for`, etc.) are safe from any
  thread, including while `define` is running on another thread.
- **Sequence generation** (`generate`, `generate-list`, inline `sequence`
  attributes invoked during a build) is atomic. Every value emitted by a
  given sequence is unique and ordered consistently with calls.
- **`rewind-sequences`** is safe to call concurrently with `generate` (a
  rewind racing a generate is well-defined: either the generate sees the
  pre-rewind counter or the post-rewind one — never a corrupted state).
- **`build` / `create` / `build-stubbed` / `attributes-for`** across threads
  produce independent results. The per-build evaluator is created fresh for
  each invocation, and the build chain used for cycle detection is
  thread-local via a dynamic variable.

## What is *not* thread-safe

- Concurrent `define` and `modify` calls serialise behind a single registry
  lock, but they do not coordinate semantically: if two threads each try to
  define a factory named `'user'`, the first wins and the second raises
  `X::ORM::Factory::DuplicateFactory`. Treat definition as a one-time
  setup phase.
- The detected persistence adapter (`ORM::Factory.persistence`) caches its
  result across the process. Setting an adapter from multiple threads at
  startup races; pick one thread (typically `main`) to call
  `set-persistence` / `reset-persistence`.
- The adapter itself is not necessarily thread-safe — the `ORM::ActiveRecord`
  adapter shares a single connection process-wide and inherits AR's
  concurrency model.

## Recommended pattern

The intended model is **definition at startup, builds concurrently**:

```perl6
# Main thread, before any workers spin up
define {
  .sequence: 'serial';
  .factory: 'user', {
    .fname:  'Greg';
    .serial: { ORM::Factory.generate('serial') };
  };
};

# Workers can now build concurrently
my @users = (^100).hyper(:degree(8)).map({
  ORM::Factory.build('user');
}).list;

@users.map(*.serial).unique.elems == 100;   # always
```

## Verifying it

The `specs/factory/concurrency-spec.raku` suite exercises the guarantees
above with hypered map blocks. If you change the registry, sequence, or
build code, run that spec across all three adapters before merging.
