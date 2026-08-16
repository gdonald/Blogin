#!/usr/bin/env bash
#
# List the lines the spec suite never ran, per file.
#
#   ./scripts/uncovered.sh              every file
#   ./scripts/uncovered.sh markdown     only files whose name contains "markdown"
#
# scripts/coverage.sh reports the percentage. This says which lines are behind
# it, which is what tells you whether one is a missing spec or a path no test
# can reach.
#
# Run scripts/coverage.sh first: this reads the profile it leaves behind, from
# the same BLOGIN_COVERAGE_BUILD_DIR that script writes to.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

pattern="${1:-}"

build_dir="${BLOGIN_COVERAGE_BUILD_DIR:-build/coverage}"
profile="$build_dir/blogin.profdata"

if [[ ! -f "$profile" ]]; then
  echo "no profile at $profile" >&2
  echo "run ./scripts/coverage.sh first" >&2
  exit 1
fi

# The same selection coverage.sh makes, so both read the toolchain that wrote
# the profile rather than whichever llvm-cov happens to be first on PATH.
if command -v xcrun >/dev/null 2>&1; then
  cov="xcrun llvm-cov"
elif command -v llvm-cov-21 >/dev/null 2>&1; then
  cov="llvm-cov-21"
else
  cov="llvm-cov"
fi

mapfile -t sources < <(find src \( -name '*.cpp' -o -name '*.h' \) | sort)

# llvm-cov show prints each file as a path ending in a colon, then one line per
# source line as `number|count|text`. A count of zero is a line that never ran.
# Consecutive ones are collapsed into a range, since a gap of forty lines reads
# as one thing to go and look at rather than forty.
$cov show "$build_dir/blogin_specs" "-instr-profile=$profile" "${sources[@]}" |
  awk -v pattern="$pattern" '
    # The run still open when a file ends is part of that file, so it is closed
    # before anything is printed rather than only at the end of the input.
    function close_range() {
      if (start == 0) {
        return
      }

      spans = spans (spans == "" ? "" : ", ") (start == previous ? start : start "-" previous)
      start = 0
    }

    function flush() {
      close_range()

      if (name == "" || count == 0) {
        return
      }

      printf "%s: %d lines: %s\n", name, count, spans
    }

    /\/[^|]*:$/ {
      flush()

      name = $0
      sub(/:$/, "", name)
      sub(/.*\//, "", name)

      count = 0
      spans = ""
      start = 0
      previous = 0

      next
    }

    match($0, /^ *[0-9]+\| *[0-9]+\|/) {
      split($0, field, "|")

      line = field[1] + 0
      ran = field[2] + 0

      if (ran != 0 || index(name, pattern) == 0) {
        next
      }

      total++
      count++

      # A line that does not follow the last one closes the range it was in.
      if (start != 0 && line != previous + 1) {
        close_range()
      }

      if (start == 0) {
        start = line
      }

      previous = line
    }

    END {
      flush()

      printf "\n%d lines uncovered\n", total
    }
  '
