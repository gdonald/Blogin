#!/usr/bin/env bash
#
# Check that a release tag names the version the project was built from.
#
#   scripts/check-version.sh v0.9.0
#   scripts/check-version.sh            # uses $GITHUB_REF_NAME
#
# The version has one home, project(VERSION) in CMakeLists.txt, and reaches the
# binary from there as BLOGIN_VERSION. A tag is the other half, and nothing in
# git makes the two agree. Tagging v0.9.0 on a tree that still says 0.8.0 would
# publish binaries reporting the wrong version under the right name, which is
# the kind of mistake nobody notices until someone reports a bug against a
# version that was never released.
#
# So the release workflow runs this before it builds anything.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

tag="${1:-${GITHUB_REF_NAME:-}}"

if [[ -z "$tag" ]]; then
  echo "usage: scripts/check-version.sh <tag>" >&2
  exit 2
fi

# A tag is written v0.9.0 and a version 0.9.0.
wanted="${tag#v}"

declared="$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\)[[:space:]]*$/\1/p' \
  CMakeLists.txt | head -1)"

if [[ -z "$declared" ]]; then
  echo "cannot find project(VERSION ...) in CMakeLists.txt" >&2
  exit 1
fi

if [[ "$declared" != "$wanted" ]]; then
  cat >&2 <<TEXT
tag '$tag' does not match the version in CMakeLists.txt

  tag says          $wanted
  CMakeLists.txt    $declared

Set project(VERSION $wanted) in CMakeLists.txt, commit, and move the tag onto
that commit.
TEXT
  exit 1
fi

echo "ok: tag $tag matches project(VERSION $declared)"
