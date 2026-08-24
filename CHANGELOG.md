# Changelog

## 0.9.3

### Hardening and testing

- Mutation testing, through `scripts/mutate.py`: it changes one operator in the
  library, rebuilds that translation unit, and runs the specs, so a mutant the
  suite still passes names behaviour nothing asserts on. A weekly job runs a
  larger sample against a floor, and `scripts/mutants-equivalent.txt` records
  the mutations that change no behaviour, with the reading behind each claim.
- The specs the survivors pointed at: paragraphs ending at a following list or
  block quote, ordered lists starting at something other than one, empty list
  items, emphasis closing against a multi-byte punctuation mark, reference
  labels matched in another case, and `script`, `pre`, `style`, and `textarea`
  blocks ending at their closing tag.
- The spec suite runs under Valgrind's memcheck, through `scripts/valgrind.sh`
  and a `valgrind` stage in `scripts/test.sh` and CI. Memcheck reports reads of
  uninitialised memory, which neither sanitizer sees, and counts every
  allocation against its free.
- The code CodeQL flagged is fixed, and `scripts/codeql.sh` runs the same query
  suite locally that the repository's security tab reports.
- The GCC build turns on `_GLIBCXX_DEBUG` through the new
  `BLOGIN_GLIBCXX_DEBUG` option, so an iterator used after its container
  reallocated, two iterators from different containers compared, and a range
  whose ends do not belong together each become a diagnostic. Every other build
  keeps `_GLIBCXX_ASSERTIONS`.
- `BLOGIN_WERROR` turns `-Werror` off for the one caller that needs it, the
  mutation runner, which would otherwise lose a mutant to a parentheses
  warning.
- The CommonMark spec loader read the tab arrow as two bytes rather than three,
  leaving a stray byte in every example that uses a tab. The conformance floor
  is 624.

## 0.9.2

### Fixed

- A rebuild deleted the responsive image variants while leaving each page's
  `srcset` naming them, so a site using `image-widths` served missing images
  from its second build onward.

### Changed

- Two posts claiming the same alias produce a warning naming both. The first
  one keeps the alias now.

## 0.9.1

No functional changes.

- Every GitHub Actions workflow runs read-only by default, and each action is
  pinned to a commit rather than a tag that can be moved.
- The development container pins its base image by digest and every package by
  version, so a Linux build compiles against the same toolchain each time.
- Static analysis through CodeQL, a published coverage figure, and an OpenSSF
  Scorecard run, alongside the sanitizers, clang-tidy, and fuzzing.
- Release binaries carry their build provenance attestation on the release
  itself, so a signature can be checked beside the file it covers.
- A security policy, at SECURITY.md.

## 0.9.0

The first release. Everything below is new, so this section describes what
Blogin does rather than what changed.

### Commands

- `blogin build` renders a content tree to static HTML. A rebuild does only the
  work the change requires and lands byte-identical to a build from scratch.
- `blogin serve` previews the site on loopback and rebuilds as you edit, with
  the open page reloading itself over a WebSocket. It builds into
  `.blogin-preview/`, so serving never writes the directory you deploy.
- `blogin init` scaffolds a site that builds with nothing to fill in first,
  against `none`, `bootstrap5`, `pico`, or `bulma`.
- `blogin new` writes a post with its front matter filled in.
- `blogin clean` removes both trees a build can write and refuses any target
  outside the site. Naming one with `--out` makes it the only target.
- Every option has a long and a one-letter spelling, and short options combine
  as `-fv`. `blogin --version` reports the version it was built from.

### Content

- CommonMark, with GitHub Flavored Markdown for tables, task lists,
  strikethrough, and fenced code.
- Footnotes, reference links, definition lists, and attribute lists on links and
  images.
- Math and Mermaid diagrams, parsed into markup a client-side renderer draws.
- Shortcodes, with `youtube` and `figure` built in and a `shortcodes/` directory
  for your own.
- Front matter for the title, date, slug, summary, ordering, drafts, table of
  contents, and redirect aliases.
- Summaries from front matter, a `<!--more-->` marker, or the opening block.

### Templates

- A HAML engine with an expression language of its own: no assignment, no
  user-defined functions, and no host language to call into. Anything outside it
  is an error naming the file, line, column, and construct.
- Layouts per section, by directory or by config.
- Themes, a directory of layouts and assets your own files override file by
  file.
- `cache-fragment`, which reuses a rendered fragment wherever the values it read
  are the same, so reuse cannot serve one page's markup to another.
- Data files, JSON and a YAML subset, site-wide and scoped to a directory.

### Output

- Atom, RSS, and JSON feeds, site-wide and per section, plus a sitemap,
  `robots.txt`, and a 404 page.
- Client-side search against a prebuilt index, with the script and its styling
  generated by the build.
- Taxonomies, with a page per term and an index, paginated like any listing.
- Optional extensionless URLs.
- More than one language, each built into its own subtree with translations
  matched by filename.

### Assets

- CSS and JavaScript minified, and assets named for a hash of their content so a
  far-future cache never serves a stale file.
- Responsive image variants with a `srcset`, when an image resizer is present.
- CSS framework profiles for Bootstrap 5, Pico, and Bulma, which class the same
  semantic HTML without a layout knowing which framework is selected.

### Platforms

- macOS through Homebrew, and Linux x86_64 and arm64 as statically linked
  binaries that run on any distribution.
- Windows through WSL2.
- Builds from source with clang on macOS and Linux, and with GCC 14 or newer on
  Linux.
- No runtime dependencies. Markdown, HAML, JSON, YAML, the asset pipeline, and
  the preview server are all in the binary.
