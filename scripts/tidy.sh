#!/usr/bin/env bash
#
# Run clang-tidy over every translation unit in the compile database. The check
# set lives in .clang-tidy, with a narrower one in specs/.clang-tidy, and every
# finding is an error, so this is a gate rather than a report.
#
# Pass file name patterns to narrow the run:
#
#   ./scripts/tidy.sh                 every file
#   ./scripts/tidy.sh 'src/blogin/h'  only the files whose path matches

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

build_dir="${BLOGIN_TIDY_BUILD_DIR:-build/debug}"

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
  echo "no compile database at $build_dir/compile_commands.json. Configure a build first" >&2
  echo "  cmake --preset debug" >&2
  exit 2
fi

if [[ -n "${RUN_CLANG_TIDY:-}" ]]; then
  runner="$RUN_CLANG_TIDY"
elif command -v run-clang-tidy >/dev/null 2>&1; then
  runner="run-clang-tidy"
elif [[ -x /opt/homebrew/opt/llvm/bin/run-clang-tidy ]]; then
  # Apple ships no clang-tidy, so on macOS it comes from Homebrew LLVM.
  runner="/opt/homebrew/opt/llvm/bin/run-clang-tidy"
  export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
else
  echo "no run-clang-tidy on PATH; brew install llvm, or apt install clang-tools" >&2
  exit 2
fi

command=("$runner" -p "$build_dir" -quiet)

# The compile database records the compiler's own name, not the SDK path it
# infers, so a clang-tidy from a different toolchain finds no standard headers
# unless it is told where they are.
if [[ "$(uname -s)" == "Darwin" ]]; then
  command+=(-extra-arg="-isysroot$(xcrun --show-sdk-path)")
fi

if command -v nproc >/dev/null 2>&1; then
  command+=(-j "$(nproc)")
else
  command+=(-j "$(sysctl -n hw.ncpu)")
fi

"${command[@]}" "$@"
