#!/usr/bin/env bash
#
# Run the spec suite under Valgrind's memcheck. This is the one tool in the set
# that reports a read of uninitialised memory: AddressSanitizer does not see
# that class of defect, UndefinedBehaviorSanitizer does not either, and a line
# of coverage says only that the line ran. Memcheck also counts every allocation
# against its free, so a leak fails here as well.
#
#   ./scripts/valgrind.sh              the whole suite
#   ./scripts/valgrind.sh Arena        one group, which is what to run while fixing
#
# Linux only, because that is where Valgrind runs. scripts/test.sh drives it in
# the Debian container on every host.
#
# The build is a plain debug one. Valgrind and the sanitizers each replace the
# allocator, so a binary carrying both reports nothing useful.
#
# Nine minutes for the whole suite, which is the slowest check here. Memcheck
# interprets every instruction, and --track-origins on top of that is what turns
# "this branch read uninitialised memory" into the line the memory came from.
# Take the flag off to halve the wall time when the report is not needed.
#
# The binary itself is not run here. The sanitizer stage drives it end to end
# through build and serve, which covers the paths the specs reach around.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

build_dir="${BLOGIN_VALGRIND_BUILD_DIR:-build-linux/valgrind}"

if ! command -v valgrind >/dev/null 2>&1; then
  echo "no valgrind on PATH. It is Linux-only, and the container has it:" >&2
  echo "  ./scripts/test.sh valgrind" >&2
  exit 2
fi

cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="${CXX:-clang++}"
cmake --build "$build_dir" -j"${BLOGIN_JOBS:-$(nproc)}"

# One job, because memcheck runs one thread at a time whatever it is asked for,
# and a serial run reports in the order the examples ran.
#
# A leak that memcheck calls possible is a block reachable only through an
# interior pointer, which several standard library allocators produce on
# purpose. Definite and indirect leaks are the ones that mean something.
valgrind \
  --error-exitcode=1 \
  --leak-check=full \
  --show-leak-kinds=definite,indirect \
  --errors-for-leak-kinds=definite,indirect \
  --track-origins=yes \
  "$build_dir/blogin_specs" --jobs 1 "$@"
