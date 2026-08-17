#!/usr/bin/env bash
#
# Build with source-based coverage, run the specs, and report line and branch
# coverage. Fails when line coverage drops below the floor, so the number is a
# gate rather than something printed and ignored.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

# A ratchet on the way to 100%, and it sits under whichever platform is lower,
# since each compiles code the other never sees. Raise it as the number climbs.
floor="${BLOGIN_COVERAGE_FLOOR:-97.4}"

# Branch coverage is the number that says whether both sides of a condition were
# taken. A parser can reach every line of a decision and only ever go one way,
# which is the case a line count cannot tell apart from a tested one. The floor
# is lower than the line floor because the metric is stricter, not because it
# matters less.
#
# Branch coverage is stricter, so its floor trails the line floor. Same ratchet.
branch_floor="${BLOGIN_BRANCH_FLOOR:-90.4}"

# Linux builds into a directory of its own, so a container sharing the working
# tree does not read a cache macOS wrote.
build_dir="${BLOGIN_COVERAGE_BUILD_DIR:-build/coverage}"

if command -v xcrun >/dev/null 2>&1; then
  profdata="xcrun llvm-profdata"
  cov="xcrun llvm-cov"
elif command -v llvm-profdata-21 >/dev/null 2>&1; then
  profdata="llvm-profdata-21"
  cov="llvm-cov-21"
else
  profdata="llvm-profdata"
  cov="llvm-cov"
fi

if [[ "$build_dir" == "build/coverage" ]]; then
  cmake --preset coverage
else
  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DBLOGIN_COVERAGE=ON
fi

cmake --build "$build_dir" -j

rm -f "$build_dir"/*.profraw "$build_dir"/blogin.profdata

LLVM_PROFILE_FILE="$build_dir/specs.profraw" "$build_dir/blogin_specs" "$@"

$profdata merge -sparse "$build_dir/specs.profraw" -o "$build_dir/blogin.profdata"

# The library is what gets measured. Spec files and the harness would inflate
# the number without saying anything about the code under test.
sources=$(find src -name '*.cpp' -o -name '*.h' | sort)

# shellcheck disable=SC2086
$cov report "$build_dir/blogin_specs" \
  -instr-profile="$build_dir/blogin.profdata" \
  -show-branch-summary \
  $sources

# shellcheck disable=SC2086
totals=$($cov export "$build_dir/blogin_specs" \
  -instr-profile="$build_dir/blogin.profdata" \
  -summary-only \
  $sources |
  python3 -c 'import json,sys; t=json.load(sys.stdin)["data"][0]["totals"]; print(t["lines"]["percent"], t["branches"]["percent"])')

line_coverage="${totals%% *}"
branch_coverage="${totals##* }"

printf '\nline coverage:   %.2f%% (floor %s%%)\n' "$line_coverage" "$floor"
printf 'branch coverage: %.2f%% (floor %s%%)\n' "$branch_coverage" "$branch_floor"

below=0

if ! python3 -c "import sys; sys.exit(0 if float('$line_coverage') >= float('$floor') else 1)"; then
  echo "line coverage below floor" >&2
  below=1
fi

if ! python3 -c "import sys; sys.exit(0 if float('$branch_coverage') >= float('$branch_floor') else 1)"; then
  echo "branch coverage below floor" >&2
  below=1
fi

# The same measurement in the format a coverage service reads, written beside
# the profile rather than uploaded from here. Nothing local needs it, and it
# costs one export, so CI has a file to hand over without a second run.
# shellcheck disable=SC2086
$cov export "$build_dir/blogin_specs" \
  -instr-profile="$build_dir/blogin.profdata" \
  -format=lcov \
  $sources > "$build_dir/coverage.lcov"

# Both are reported before either fails, so one run says everything that is
# wrong rather than hiding the second number behind the first.
if [[ "$below" -ne 0 ]]; then
  exit 1
fi

rm -f "$build_dir"/*.profraw
