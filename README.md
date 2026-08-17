# Blogin

[![CI](https://github.com/gdonald/Blogin/actions/workflows/ci.yml/badge.svg)](https://github.com/gdonald/Blogin/actions/workflows/ci.yml)
[![Fuzz](https://github.com/gdonald/Blogin/actions/workflows/fuzz-weekly.yml/badge.svg)](https://github.com/gdonald/Blogin/actions/workflows/fuzz-weekly.yml)
[![Release](https://img.shields.io/github/v/release/gdonald/Blogin)](https://github.com/gdonald/Blogin/releases/latest)
[![License](https://img.shields.io/github/license/gdonald/Blogin)](LICENSE)
[![Downloads](https://img.shields.io/github/downloads/gdonald/Blogin/total)](https://github.com/gdonald/Blogin/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)](https://en.cppreference.com/w/cpp/23)
[![Coverage](https://codecov.io/gh/gdonald/Blogin/branch/main/graph/badge.svg)](https://codecov.io/gh/gdonald/Blogin)
[![Scorecard](https://api.scorecard.dev/projects/github.com/gdonald/Blogin/badge)](https://scorecard.dev/viewer/?uri=github.com/gdonald/Blogin)

An insanely fast static site generator written in C++.

Point it at a directory of Markdown and it writes a directory of HTML you can
host anywhere. No database, no runtime, nothing to install alongside it.

What it produces is a complete site: a page per post, a paginated listing per
section, tag and other taxonomy pages, Atom, RSS and JSON feeds, a sitemap,
`robots.txt`, a client-side search index and the script that reads it, a 404
page, redirect pages for any alias a post declares, and the stylesheets, scripts
and images the site references, minified and content-hashed if you ask for it.

**[blogin.dev](https://blogin.dev) is the documentation.** The guide, the CLI
reference, the template expression language, the HAML compatibility registry,
and the YAML subset all live there. This file covers building the binary and
working on it.

```bash
blogin init myblog        # write a complete site into an empty directory
cd myblog
blogin new "First Post"   # a dated file with front matter filled in
blogin serve              # http://127.0.0.1:3000, rebuilding as you edit
blogin build              # write the site to public/, ready to deploy
```

A rebuild only does the work the change requires. When nothing changed, nothing
is parsed, rendered, or written, and when one post changed, one post is
rendered. The result is byte-identical to a build from scratch, which is checked
against randomised edit sequences rather than a few examples.

## Install

```bash
brew tap gdonald/blogin
brew trust gdonald/blogin
brew install blogin
```

On Linux, take the binary for your architecture from the latest release. Both
are statically linked, so one file per architecture covers every distribution:

```bash
curl -LO https://github.com/gdonald/Blogin/releases/latest/download/blogin-linux-x86_64
curl -LO https://github.com/gdonald/Blogin/releases/latest/download/blogin-linux-x86_64.sha256
sha256sum -c blogin-linux-x86_64.sha256
chmod +x blogin-linux-x86_64
sudo mv blogin-linux-x86_64 /usr/local/bin/blogin
```

`blogin-macos-universal` covers Apple silicon and Intel in one file for anyone
skipping Homebrew. Fetch it with `curl` rather than a browser, since a browser
download is quarantined and macOS then refuses to run an unsigned binary.

### Verifying a download

Every release carries a `.sha256` per binary and a combined `SHA256SUMS`, which
catch a damaged download:

```bash
sha256sum -c SHA256SUMS
```

Each binary is also signed, which answers where it came from rather than
whether it arrived intact. Anyone who replaces a file can regenerate its
checksum, and nobody can forge the signature.

```bash
gh attestation verify blogin-linux-x86_64 --repo gdonald/Blogin
```

That checks a signature made by this repository's release workflow, recorded in
a public transparency log. There is no key to fetch first and no fingerprint to
confirm with anyone, because the identity being signed for is the workflow and
the commit rather than a person.

Windows has no native build. WSL2 is Linux, so install the Linux binary there.
[blogin.dev](https://blogin.dev/guide/getting-started/) covers each platform.

## Building from source

Requirements: a C++23 compiler and CMake 3.28 or newer. Ninja is optional and
used when present. The macOS deployment floor is 13.3, which is where
`std::format` on floating point has what it needs.

Releases are built with clang on both platforms, and the presets select it. GCC
14 or newer is supported on Linux and builds everything except the integer
checks and coverage, which are clang's alone. Both are tested on every push.

```bash
cmake --preset release
cmake --build build/release -j
sudo cmake --install build/release   # /usr/local/bin/blogin
```

For development work use the debug preset and run the specs:

```bash
cmake --preset debug
cmake --build build/debug -j
./build/debug/blogin_specs
```

CMake picks the platform default generator. To use Ninja everywhere:

```bash
export CMAKE_GENERATOR=Ninja
```

### Presets

| Preset     | What it is for                                                                    |
| ---------- | --------------------------------------------------------------------------------- |
| `debug`    | Everyday work                                                                     |
| `sanitize` | AddressSanitizer and UndefinedBehaviorSanitizer                                   |
| `thread`   | ThreadSanitizer                                                                   |
| `integer`  | The sanitizers plus unsigned wraparound, narrowing, stack bounds, and nullability |
| `coverage` | Source-based coverage instrumentation                                             |
| `tidy`     | clang-tidy on every file the build compiles                                       |
| `fuzz`     | libFuzzer targets                                                                 |
| `release`  | Optimized                                                                         |
| `dist`     | Optimized, statically linked, for distribution                                    |

GCC gets its own presets on Linux, since the ones above select clang:

| Preset         | What it is for                                  |
| -------------- | ----------------------------------------------- |
| `gcc`          | Everyday work                                   |
| `gcc-sanitize` | AddressSanitizer and UndefinedBehaviorSanitizer |
| `gcc-thread`   | ThreadSanitizer                                 |
| `gcc-release`  | Optimized                                       |

`BLOGIN_SANITIZE_INTEGER` and `BLOGIN_COVERAGE` are refused at configure time
under GCC, naming what to do, rather than failing on the first file of a long
build.

On macOS the `fuzz` preset needs Homebrew LLVM, because Apple clang ships no
libFuzzer runtime. The `tidy` preset and `scripts/tidy.sh` need it too, for the
same reason: Apple ships no clang-tidy.

## Dependencies

None at runtime, and none to build beyond a compiler and CMake. JSON, YAML,
templating, the HTTP and WebSocket preview server, file watching, and the test
harness are all written in this repository.

The one optional external tool is an image resizer, ImageMagick or macOS `sips`,
used for responsive images. Without one that step is skipped with a warning and
everything else builds.

Release binaries are fully static on Linux. On macOS they link only the system
libraries Apple ships in every install, since the platform has no static libc.

## Project layout

```
src/blogin/            the library, one file per concern
src/blogin/include/    its headers, one per implementation file
src/main.cpp           argument parsing to a Command, then dispatch
src/bench.cpp          the benchmark harness
specs/                 one spec file per header, plus the test harness
specs/support/         describe, context, it, let, expect, the runner
specs/corpus/          the four real sites, layouts and content, committed
specs/golden/          expected output, regenerated by a flag
specs/commonmark/      the vendored CommonMark conformance suite
fuzz/                  libFuzzer targets
docker/                the Debian development container
cmake/                 the sanitizer ignorelist and other CMake inputs
scripts/               developer entry points
```

A change goes beside the concern it belongs to: `markdown.cpp` parses,
`html.cpp` renders what it parsed, `haml.cpp` and `template.cpp` are the
template engine, `view*.cpp` is the surface a layout reads through, `site.cpp`
is the build, and `writer.cpp` is what reaches disk.

## Testing

`scripts/test.sh` is every check CI runs except fuzzing, one stage per CI job.
Run it before a commit:

```bash
./scripts/test.sh          # every stage, several at a time
./scripts/test.sh --list   # the stages, and the options
./scripts/test.sh specs    # one of them
./scripts/test.sh -j 14    # spend 14 cores across the run
./scripts/test.sh --serial # one at a time
```

The spec runner underneath it takes a filter and a job count:

```bash
./build/debug/blogin_specs              # everything
./build/debug/blogin_specs Arena        # filter by name
./build/debug/blogin_specs --jobs 8     # in parallel
./build/debug/blogin_specs --list       # names only
```

Specs live in `specs/`, one file per header, written with a small in-repo
harness offering `describe`, `context`, `it`, `before_each`, `let`, and
`aggregate_failures`.

Nothing asserts on elapsed time, and nothing fails because the machine is slow
or busy. Speed is checked through work counters, which are deterministic across
machines. Wall-clock numbers come from `scripts/bench.sh` and inform judgement
without failing a build.

A spec that has to prove something did not happen drives the code directly
rather than waiting a fixed time. The preview server takes its watcher through a
factory on `ServeOptions`, so a spec hands it a scripted sequence of changes and
asserts what a burst collapses into, with no clock, no filesystem, and no need
for a fast machine.

Every deadline in the specs bounds how long a failing example takes rather than
deciding anything, so a slower machine waits longer and still passes. The suite
runs clean on two cores.

`scripts/coverage.sh` reports line and branch coverage and fails below a floor,
on macOS and in the Debian container both. `scripts/uncovered.sh` lists the
lines behind the number, so an uncovered line can be told apart as a missing
spec or a path no test can reach.

### Fuzzing

One target per parser that reads bytes nobody on this side wrote: markdown,
templates, HAML, JSON, YAML, configuration, expressions, shortcodes, HTTP
requests, the request-path-to-file resolution the preview server does, WebSocket
frames, and the UTF-8 boundary math under summaries and search. Each one checks
invariants rather than only surviving, so an input that produces wrong output is
a finding and not only one that crashes.

```bash
./scripts/fuzz.sh                  # replay every corpus and regression input
./scripts/fuzz.sh explore 900      # mutate for 900 seconds per target
./scripts/fuzz.sh explore 900 haml # one target
./scripts/fuzz.sh minimize         # shrink each corpus to the same coverage
```

Inputs live in three places. `fuzz/seeds/` is written by hand, one file per
construct, and documents the format. `fuzz/corpus/` is grown by the fuzzer and
is the only one `minimize` prunes. `fuzz/regressions/` holds inputs that once
failed. Replay runs all three and takes about a second, on every pull request.
A weekly job explores for a minute per target, one runner each, with the corpus
each run grows cached for the next.

### Hardening

Every preset builds with the standard library's bounds checks on, a stack
protector, and locals filled rather than left indeterminate. Release uses the
cheaper setting of each, so a downloaded binary traps on an out-of-range
subscript instead of reading past the container. The flags that exist on only
some platforms are probed at configure time, and CMake then builds and runs a
throw and catch to check that what it selected produces a binary that works.

### Linux

Linux work happens in the Debian container, so what CI runs and what you run are
the same thing:

```bash
./scripts/test-linux.sh    # configure, build, and run the specs in the container
./docker/dev.sh            # a shell in it
```

The container also carries g++, which is how GCC is supported and tested. A CI
job builds plain and with the sanitizers and runs the specs against both. A
different frontend reports things clang accepts, and it builds against libstdc++
rather than libc++.

```bash
./scripts/test.sh gcc      # both GCC builds and the specs, in the container
```

On a Linux machine, the presets do it without the container:

```bash
cmake --preset gcc
cmake --build build/gcc -j
./build/gcc/blogin_specs
```

### Scripts

Everything a change needs:

| Script                       | What it does                                                               |
| ---------------------------- | -------------------------------------------------------------------------- |
| `scripts/test.sh`            | Everything CI checks except fuzzing. Run before a commit                   |
| `scripts/tidy.sh`            | clang-tidy over every translation unit, failing on any finding             |
| `scripts/coverage.sh`        | Coverage report, failing below a floor                                     |
| `scripts/uncovered.sh`       | The lines behind the coverage number, per file                             |
| `scripts/repin-container.sh` | Move the container's pinned package versions and base image forward        |
| `scripts/fuzz.sh`            | Replay the fuzz corpus, explore for new inputs, or minimize what was found |
| `scripts/sanitize-cli.sh`    | Drive the binary itself under the sanitizers, build through serve          |
| `scripts/test-linux.sh`      | Build and test on Linux in the Debian container                            |
| `scripts/check-static.sh`    | Assert a release binary runs with nothing installed                        |
| `scripts/regold.sh`          | Regenerate golden files, then show the diff                                |
| `scripts/bench.sh`           | Wall-clock measurements, never a gate                                      |
| `scripts/refresh-corpus.sh`  | Re-copy the real sites into the test corpus                                |
| `scripts/synth-corpus.sh`    | Generate a large synthetic site for scaling work                           |
| `docker/dev.sh`              | Shell in the Debian development container                                  |

### Release scripts

Cutting a release is the maintainer's job, and the workflow in
`.github/workflows/release.yml` calls these. Nothing here is needed to
contribute.

| Script                       | What it does                                                             |
| ---------------------------- | ------------------------------------------------------------------------ |
| `scripts/check-version.sh`   | Check a release tag against project(VERSION) in CMakeLists               |
| `scripts/changelog-notes.sh` | Print one version's section of CHANGELOG.md, for the release notes       |
| `scripts/release.sh`         | Build the release binaries into `dist/`, with a checksum beside each     |
| `scripts/formula.sh`         | Write the Homebrew formula for a release, filled in from those checksums |

## Contributing

Bug reports, patches, and questions are welcome. The workflow is the usual one.

1. Fork the repository on GitHub and clone your fork.
2. Branch from `main`: `git switch -c fix-summary-truncation`.
3. Make the change, with a spec that fails without it.
4. Run `./scripts/test.sh`, which is every check CI runs except fuzzing.
5. Push the branch to your fork and open a pull request against `main`.

A pull request describes what changed and why. Title it for the behaviour, not
for a file or a phase number.

Every CI job runs one stage of `scripts/test.sh`, so a passing run locally is
the same set of checks CI will run:

```bash
./scripts/test.sh          # every stage, several at a time
./scripts/test.sh --list   # the stages, and the options
./scripts/test.sh specs    # one of them
./scripts/test.sh native   # only what needs no container
./scripts/test.sh --serial # one at a time
```

Stages run concurrently, each in its own build directory. `-j` is the core
budget for the whole run, defaulting to four fewer than the machine has, and
two cores per stage decides how many run at once. Output is kept for a stage
that fails and dropped for one that passes.

The container stages need Docker running. A skipped stage is reported and exits
non-zero, since CI runs it either way. Fuzzing is the one check left out, and
`./scripts/fuzz.sh` runs it.

Documentation for users lives at [blogin.dev](https://blogin.dev), in a separate
repository. A change that alters behaviour a user can see needs the matching
page updated there in the same batch of work.

The rest of this section is the style contract the codebase is written to.

### Layout

```
src/blogin/include/*.h    public headers, one per concern
src/blogin/*.cpp          implementations
src/main.cpp              the CLI entry point
src/bench.cpp             the benchmark driver
specs/*_spec.cpp          one spec file per header
specs/support/            the spec harness, not part of the library
specs/corpus/             the four real sites, committed
specs/golden/             expected output, committed
scripts/                  developer entry points
docker/                   the Debian development container
fuzz/                     libFuzzer entry points
```

A header and its implementation share a name. A header gets a spec file with the
same stem. `spec.h` lives with the harness rather than in the include directory,
because it is not part of the library's surface.

### Naming

- `snake_case` for functions, variables, and files.
- `PascalCase` for types.
- `snake_case` with a trailing underscore for private data members.
- Descriptive names, including in small scopes. No single letters, no `i`, `n`,
  `tmp`, `ctx`. `index`, `count`, `character`, `context`.
- Functions are named for what they produce or do, not for how. `parse_markdown`,
  not `do_parse`.

### Errors

Exceptions at the build boundary: a failure prints a message and exits non-zero.
`std::expected` for parse results a caller inspects and recovers from.

An error message names the thing that failed. A path, a file and line, the
offending construct. "cannot read /some/path" rather than "read error".

### Memory and lifetimes

The performance work rests on two decisions the type system does not enforce, so
they are stated here and checked by sanitizers.

**Arenas.** Parse trees are bump-allocated and released whole. Types placed in an
arena must be trivially destructible, which `Arena::create` asserts. Nothing in
an arena owns heap memory of its own.

An arena is not thread safe. Each thread owns one. `reset()` keeps the largest
block and drops the rest, so a thread parsing many posts pays for one set of
blocks rather than one per post. Every pointer and view handed out before a reset
dangles after it, so reset only when the whole tree is finished with.

AddressSanitizer sees the block, not the nodes inside it, so on its own it cannot
tell one node from the next and an overrun of a few bytes goes unreported. The
arena poisons its blocks and unpoisons each allocation to its exact size, with a
gap between neighbours, so leaving a node reports and a pointer used after a
reset reports too. None of that is compiled into a build without the sanitizer,
where the layout is a plain bump.

**Views.** `std::string_view` refers into a buffer it does not own. The owner is
always visible: a `Source` owns the text, and the tree parsed from it must not
outlive it. A function taking a view and storing it must document what the view
must outlive.

Strings are materialized only when escaping or transforming produces new bytes.

### Concurrency

No shared mutable state, rather than locks around shared mutable state. Data read
by many threads is immutable after construction, and anything mutable is
per-thread and private. No hot path takes a lock.

ThreadSanitizer runs the whole suite on both platforms, so a race fails the
build. Where a thread hands work to a callback the platform runs, teardown has
to wait for the callback rather than assume unregistering stopped it.

Work counters are the one exception, and they are relaxed atomics rather than
free. They increment at coarse granularity only: per file, per page, per
template. Never inside a parse or render inner loop, where they would be
contended on every iteration.

### Work counters

Counters record work done, and the spec suite asserts on them. They are what
makes an algorithmic regression visible without measuring time.

A counter is added to the `Counter` enum and to the name table beside it. Its
name is what the assertion reads, so it says what was counted:
`templates_compiled`, not `template_count`.

### Specs

- One spec file per header. `describe` names the unit, `context` names a
  situation, `it` names an observable behaviour. Read together they form a
  sentence.
- Prefer `let` over a local for a subject used across examples. Declare it in the
  narrowest scope that covers the examples using it. A nested `context` with its
  own `let` shadows an outer one.
- Ideally one `expect` per example. When one expensive setup needs several
  assertions, wrap them in `aggregate_failures` so a failure reports all of them
  rather than the first.
- Descriptions state observable behaviour, spelled out, no abbreviations. No
  numbering.
- Never assert on elapsed time. Assert on work counters. Wall-clock numbers
  belong in the benchmark driver.
- `spec::serial()` marks a group that needs exclusive access to process-global
  state: the work counters, the shared scratch directory, the working directory.
  It is not for fixtures that should have been separate.
- Nothing reads outside the repository. The corpus is committed for this reason.

### Static analysis

clang-tidy runs over every translation unit and every finding is an error, so a
clean run is the only passing state. `.clang-tidy` holds the check set and
`specs/.clang-tidy` narrows it for the spec files, where the harness macros
trigger five checks that mean nothing there.

A check is enabled when what it finds is a defect or a mechanical cleanup, and
disabled when it enforces a style opinion this codebase does not share. Each
disabled check carries a one-line reason next to it. Adding a check means fixing
what it finds in the same change, not leaving it on with a backlog.

Silencing a single finding takes a `NOLINTNEXTLINE(check-name)` with the reason
on the same line. There is no blanket `NOLINT`.

```bash
./scripts/tidy.sh                 # every file, using build/debug
./scripts/tidy.sh 'src/blogin/h'  # only files whose path matches

cmake --preset tidy               # or tidy on every compile, incrementally
cmake --build build/tidy -j
```

### Formatting

- Two-space indent, no tabs.
- Blank lines separate distinct intents: setup, action, assertion. Not every
  statement, and not none.
- Comments explain why, not what. They are short and factual. A comment that
  restates the code is deleted.

## License

MIT. See `LICENSE`.
