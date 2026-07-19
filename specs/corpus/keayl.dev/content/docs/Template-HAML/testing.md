---
title: Tests
slug: testing
order: 21
description: Template::HAML has two parallel test suites:
toc: true
---
Template::HAML has two parallel test suites:

* `t/` — `prove6`-driven `.rakutest` files, one per feature area.
* `specs/` — BDD-Behave specs grouped by feature directory (`specs/attrs/`,
  `specs/codegen/`, ...). Each `.rakutest` under `t/` has a paired
  `*-spec.raku` under `specs/`.

The two suites cover the same behavior from different angles: the
`.rakutest` files test the public API at the unit level; the BDD specs
describe expected behavior in a `describe`/`context`/`it` form and are
the source of truth for the documented syntax.

## Run

The canonical way to run the full suite is the project's driver script:

```shell
./test.raku
```

It runs three stages in order:

1. `prove6 -j<cores> -Ilib t/` against the default direct-emit codegen.
2. `prove6 -j<cores> -Ilib t/` with `HAML_DEFAULT_EMIT=ast` so every test
   also runs against the AST walker.
3. `behave --exclude-tag benchmark` for the BDD specs (benchmarks are
   excluded from the default run; see [Benchmarks](/docs/Template-HAML/benchmarks)).

Author-only checks are gated behind `AUTHOR_TESTING=1`; `test.raku` sets
that automatically, so the META validation in `t/0000-meta.rakutest` and
`t/0010-meta-provides.rakutest` runs as part of the local suite.

To run a single stage manually:

```shell
prove6 -Ilib t/                                  # direct emit
HAML_DEFAULT_EMIT=ast prove6 -Ilib t/            # AST walker
behave --exclude-tag benchmark                   # BDD specs
```

Install test-depends from `META6.json` first if you don't have them:

```shell
zef install --deps-only --/test --test-depends .
```

## Golden-file tests

`t/0410-golden-files.rakutest` iterates `t/fixtures/golden/*.haml` against
`t/fixtures/golden/*.html` and asserts the rendered output matches the
recorded golden output. Add a new fixture by dropping a matched
`.haml`/`.html` pair into that directory.

## CI

Every push and every pull request runs the workflow at
`.github/workflows/ci.yml` on `ubuntu-latest`. The workflow installs
Rakudo, BDD-Behave, project test-deps, and `App::Prove6`, then runs
`./test.raku` followed by `behave --benchmark specs/bench/` to exercise
the benchmark harness as a smoke test (with `HAML_BENCH_ITERS=10`).
