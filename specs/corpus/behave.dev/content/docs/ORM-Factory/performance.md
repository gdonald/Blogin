---
title: Performance
slug: performance
order: 26
description: build-stubbed exists specifically to avoid the database; that advantage should be real and stay real across releases.
toc: true
---
`build-stubbed` exists specifically to avoid the database; that advantage
should be real and stay real across releases.

## Guarantees

- **`build-stubbed` makes zero `persist()` calls.** The adapter sees one
  `instantiate` and one `stub`, and no `persist`. This is asserted by
  `specs/factory/performance-spec.raku`, so accidentally routing
  `build-stubbed` through `persist` will fail CI.
- **`build` makes zero `persist()` calls.** Same idea — `build` is the
  no-save sibling of `create` and that contract is asserted.
- **`attributes-for` makes zero adapter calls.** It returns the resolved
  attribute hash without ever touching `instantiate`, `persist`, or `stub`.
- **Evaluator memoisation.** Each attribute (transient or persisted) is
  computed at most once per build, even when referenced from multiple
  callbacks or dependent attributes.

## The benchmark harness

`bin/bench` runs each of the four strategies against the same factory and
prints average wall-clock time per record. Default is 1000 records, 3
repeats:

```
$ raku bin/bench
ORM::Factory benchmarks (count=1000, repeat=3)
------------------------------------------------------------
  attributes-for    avg  XX.XXms  (XX.XX µs / op)
  build             avg  XX.XXms  (XX.XX µs / op)
  build-stubbed     avg  XX.XXms  (XX.XX µs / op)
  create            avg  XX.XXms  (XX.XX µs / op)
```

Use `--count` and `--repeat` to scale the workload:

```
raku bin/bench --count=5000 --repeat=5
```

The harness uses the in-memory generic adapter (no DB), so the difference
between `create` and the others reflects only the `save-bang` call cost.

## Regression guard

The performance spec includes a coarse guard: `build-stubbed-list` is asserted
to be no more than 2× slower than `create-list` over 200 records. That gap
catches order-of-magnitude regressions without flaking on machine noise. For
finer regression tracking, run `bin/bench` before and after a change.
