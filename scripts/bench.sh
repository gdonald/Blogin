#!/usr/bin/env bash
#
# Wall-clock measurements, recorded so a slowdown is visible over time.
#
# Never a gate. Timing on a shared machine is noise, which is why the spec suite
# asserts on work counters instead. These numbers inform judgement about whether
# the thing is fast enough; they do not fail a build.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

build_dir="build/release"

cmake --preset release >/dev/null
cmake --build "$build_dir" --target blogin_bench -j >/dev/null

if [[ ! -d build/synth-corpus ]]; then
  echo "==> generating the synthetic corpus (once)"
  ./scripts/synth-corpus.sh >/dev/null
fi

echo "==> $(date -u '+%Y-%m-%dT%H:%M:%SZ')  $(uname -s) $(uname -m)"
echo

"$build_dir/blogin_bench" "$@"
