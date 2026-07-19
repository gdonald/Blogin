#!/usr/bin/env bash
#
# Re-copy the real sites into specs/corpus/.
#
# The corpus is committed so that tests never read outside the repository. A
# working copy of the sites does not exist on a CI runner or inside the Debian
# container, and a test that depends on one is not reproducible.
#
# What is copied is the material a build renders from: layouts, content, data,
# shortcodes, and blogin.json. What is not copied is assets/ and static/, which
# are photographs, fonts, and icons running to about 180 MB across the four
# sites and saying nothing about template or Markdown behaviour. Responsive
# image work gets its own small image fixtures instead.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_root="${BLOGIN_SITES:-$HOME/workspace/blogin-sites}"
target_root="$root/specs/corpus"

sites=(behave.dev blogin.dev gregdonald.com keayl.dev)
directories=(layouts content data shortcodes)

if [[ ! -d "$source_root" ]]; then
  echo "no sites at $source_root. Set BLOGIN_SITES to override" >&2
  exit 2
fi

skipped=0

for site in "${sites[@]}"; do
  if [[ ! -d "$source_root/$site" ]]; then
    echo "warning: $site not found at $source_root, skipping" >&2
    skipped=$((skipped + 1))
    continue
  fi

  rm -rf "${target_root:?}/$site"
  mkdir -p "$target_root/$site"

  for directory in "${directories[@]}"; do
    [[ -d "$source_root/$site/$directory" ]] || continue

    cp -R "$source_root/$site/$directory" "$target_root/$site/$directory"
  done

  if [[ -f "$source_root/$site/blogin.json" ]]; then
    cp "$source_root/$site/blogin.json" "$target_root/$site/blogin.json"
  fi

  # Nothing binary should have come along. If it did, say which file rather
  # than quietly carrying a photograph into the repository.
  while IFS= read -r file; do
    echo "warning: unexpected non-text file $file" >&2
    skipped=$((skipped + 1))
  done < <(find "$target_root/$site" -type f \
    ! -name '*.md' ! -name '*.haml' ! -name '*.json' ! -name '*.yaml' ! -name '*.yml' \
    ! -name '*.html' ! -name '*.txt' ! -name '.keep')
done

echo "==> corpus now holds:"

for site in "${sites[@]}"; do
  [[ -d "$target_root/$site" ]] || continue

  printf '  %-16s %5s files %6s KB\n' \
    "$site" \
    "$(find "$target_root/$site" -type f | wc -l | tr -d ' ')" \
    "$(find "$target_root/$site" -type f -exec du -ck {} + | tail -1 | cut -f1)"
done

if [[ "$skipped" -gt 0 ]]; then
  echo
  echo "$skipped item(s) were skipped or flagged above." >&2
fi
