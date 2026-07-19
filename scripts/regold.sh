#!/usr/bin/env bash
#
# Regenerate golden files, then show what moved.
#
# Regeneration is always an explicit act. Nothing rewrites a golden file as a
# side effect of a failing run, so a golden always reflects output somebody
# chose to accept.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

preset="${1:-debug}"
build_dir="build/$preset"

if [[ ! -d "$build_dir" ]]; then
  cmake --preset "$preset"
fi

cmake --build "$build_dir" -j

BLOGIN_REGOLD=1 "$build_dir/blogin_specs" "${2:-}"

echo
echo "==> golden files now on disk:"
find specs/golden -type f | sort

if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo
  echo "==> review this diff before committing:"
  git --no-pager diff --stat -- specs/golden || true
fi
