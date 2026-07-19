#!/usr/bin/env bash
#
# Print one version's section of CHANGELOG.md, for the GitHub release notes.
#
#   scripts/changelog-notes.sh v0.9.0
#   scripts/changelog-notes.sh            # uses $GITHUB_REF_NAME
#
# The release workflow feeds this to `gh release create --notes-file`, so the
# notes on GitHub and the section in the file are the same words. Generating
# notes from commit subjects instead would describe the work rather than the
# release, and would say nothing to someone deciding whether to upgrade.
#
# Exits non-zero when the version has no section, which is what stops a release
# going out with nothing written about it.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

tag="${1:-${GITHUB_REF_NAME:-}}"

if [[ -z "$tag" ]]; then
  echo "usage: scripts/changelog-notes.sh <tag>" >&2
  exit 2
fi

version="${tag#v}"

# From the heading for this version to the line before the next heading.
notes="$(awk -v want="## $version" '
  $0 == want { collecting = 1; next }
  collecting && /^## / { exit }
  collecting { print }
' CHANGELOG.md)"

# Blank lines at either end are an accident of where the headings sit.
notes="$(printf '%s' "$notes" | sed -e '/./,$!d' | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}')"

if [[ -z "$notes" ]]; then
  echo "CHANGELOG.md has no section for $version" >&2
  echo "Add a '## $version' heading describing the release, then tag again." >&2
  exit 1
fi

printf '%s\n' "$notes"
