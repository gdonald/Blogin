---
title: Benchmarks
slug: benchmarks
order: 22
description: The repo ships two parallel bench harnesses that compare the two codegen paths — the direct-emit codegen (default) and the AST walker (:emit<ast>, see...
toc: true
---
The repo ships two parallel bench harnesses that compare the two codegen
paths — the direct-emit codegen (default) and the AST walker
(`:emit<ast>`, see [API](/docs/Template-HAML/api#direct-emit-codegen-default)) — on a
fixed set of representative templates:

* `specs/bench/render-spec.raku` — BDD-Behave spec, tagged
  `:tag<benchmark>` so it is skipped by the default test run and driven
  separately via `behave --benchmark specs/bench/`.
* `t/0790-bench.rakutest` — `prove6`-style harness, skipped unless
  `HAML_RUN_BENCH=1` is set in the environment.

Both files share the same template cases and the same render setup, so
either entry point produces a comparable measurement.

## Run

From the project root, driving the BDD-Behave spec:

```shell
behave --benchmark specs/bench/
```

Driving the `prove6` harness:

```shell
HAML_RUN_BENCH=1 prove6 -Ilib t/0790-bench.rakutest
```

Both harnesses honor `HAML_BENCH_ITERS` to override the iteration count
(default `5000` for the BDD spec, `100` for the `prove6` test):

```shell
HAML_BENCH_ITERS=20000 behave --benchmark specs/bench/
HAML_BENCH_ITERS=1000 HAML_RUN_BENCH=1 prove6 -Ilib t/0790-bench.rakutest
```

## Output

The `prove6` harness prints one TAP diagnostic per case:

```
# static tags only                  iters=100  ast=8.41ms  direct=2.31ms  speedup=3.64x
# inline expressions                iters=100  ast=4.92ms  direct=1.43ms  speedup=3.44x
# for-loop over a list              iters=100  ast=44.7ms  direct=12.8ms  speedup=3.49x
...
```

The BDD-Behave benchmark mode renders comparable per-case tables driven
by the `benchmark 'ast' { ... }` / `benchmark 'direct' { ... }` blocks
inside the spec.

Each case is warmed once for both paths before the timed loop, so the
per-template precompile/EVAL cost is excluded from the timings — what's
being measured is steady-state render speed.

## Cases

The cases are declared in [`specs/bench/render-spec.raku`](https://github.com/gdonald/Template-HAML/blob/main/specs/bench/render-spec.raku)
(mirrored by `t/0790-bench.rakutest`). Each entry is a `(name, src, locals)`
record. Add a case by appending to the list — the harnesses pick it up
automatically.

## Caveats

- The numbers are wall-clock and depend on machine load. Run a few times if
  the comparison is close.
- The harness times `HAML.render(:src, ...)` end-to-end. The first render
  for each path warms internal caches; subsequent renders reuse the
  cached closure, so the times reflect the steady-state cost, not first-render
  cost.
- `:emit<direct>` covers the same template-level features as the AST
  renderer (trim modifiers, `remove-whitespace`, preserved tags with
  children, tab-offset propagation). The only path it does not yet drive
  is streaming (`HAML.render-supply`), which stays on the AST renderer
  regardless of `:emit`. See
  [API: Feature parity with the AST renderer](/docs/Template-HAML/api#feature-parity-with-the-ast-renderer).
