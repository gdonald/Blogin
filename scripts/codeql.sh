#!/usr/bin/env bash
#
# Run CodeQL's security-and-quality suite over the whole tree. That is the same
# suite .github/workflows/codeql.yml runs on every push to main, so a finding
# here is an alert on the repository's security tab, and every finding is a
# failure rather than a report.
#
#   ./scripts/codeql.sh
#
# CodeQL reads a compiler as it runs rather than parsing the source, so this
# configures a build of its own and watches it. The database it leaves behind
# can be queried directly:
#
#   codeql query run --database=build/codeql-database some.ql
#
# The compiler here is whatever the host has, while the workflow uses GCC 14 on
# Linux. The queries and their answers are the same for everything that compiles
# on both. Code behind a platform conditional is only analyzed where it
# compiles, so watcher.cpp's inotify branch is covered in CI and not here.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

build_dir="${BLOGIN_CODEQL_BUILD_DIR:-build/codeql}"
database_dir="${BLOGIN_CODEQL_DATABASE_DIR:-build/codeql-database}"
results="$build_dir/results.csv"

# The workflow asks for security-and-quality, which is the default queries plus
# the ones covering correctness rather than only security.
suite="codeql/cpp-queries:codeql-suites/cpp-security-and-quality.qls"

if ! command -v codeql >/dev/null 2>&1; then
  echo "no codeql on PATH; brew install --cask codeql, or take the release from" >&2
  echo "https://github.com/github/codeql-cli-binaries/releases" >&2
  exit 2
fi

if command -v nproc >/dev/null 2>&1; then
  jobs="${BLOGIN_JOBS:-$(nproc)}"
else
  jobs="${BLOGIN_JOBS:-$(sysctl -n hw.ncpu)}"
fi

# A release build, which is what the workflow analyzes, and a build directory of
# its own so a stage running beside this one is not tracing this compile.
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release

# The database holds what the traced build compiled, so the build has to be a
# full one. An incremental build compiles the files that changed, and the
# database is then a database of those files: every query comes back clean about
# the code it never saw. --clean-first is what keeps a second run honest.
#
# The database is rebuilt rather than updated for the same reason. CodeQL has no
# incremental mode, and a stale database answers about code that is gone.
codeql database create "$database_dir" \
  --language=c-cpp \
  --source-root=. \
  --overwrite \
  --command="cmake --build $build_dir --clean-first -j $jobs"

# --download fetches the query pack the first time and is a no-op afterwards.
codeql database analyze "$database_dir" "$suite" \
  --download \
  --threads="$jobs" \
  --format=csv \
  --output="$results"

if [[ -s "$results" ]]; then
  echo
  echo "CodeQL findings, one per line, as rule, description, severity, message, then location:"
  echo
  cat "$results"
  echo
  echo "Each of these becomes an alert on the security tab once this is pushed."

  exit 1
fi

echo
echo "CodeQL: no findings from $suite"
