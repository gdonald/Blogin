#!/usr/bin/env bash
#
# Build and run the fuzz targets.
#
#   ./scripts/fuzz.sh                   replay every checked-in corpus, no mutation
#   ./scripts/fuzz.sh replay markdown   replay one target's corpus
#   ./scripts/fuzz.sh explore 900       mutate for 900 seconds per target, keeping finds
#   ./scripts/fuzz.sh explore 900 haml  one target
#   ./scripts/fuzz.sh minimize          shrink each corpus to the smallest set
#     covering the same edges, leaving the hand-written seeds and the
#     regression inputs alone
#
# Replay is the mode that belongs on every pull request: it is deterministic,
# takes seconds, and fails when an input that used to be handled stops being
# handled. Explore is the mode that finds new inputs, and what it finds is
# written back into the corpus for replay to keep checking.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

build_dir="${BLOGIN_FUZZ_BUILD_DIR:-build/fuzz}"
corpus_root="fuzz/corpus"

# Written by hand and never minimized: seeds document the format, regressions
# pin an input that once failed. Both are replayed and both seed exploration.
# Only the corpus is machine-grown, and only the corpus is minimized.
seed_root="fuzz/seeds"
regression_root="fuzz/regressions"
dict_root="fuzz/dict"
artifacts="${BLOGIN_FUZZ_ARTIFACTS:-fuzz/artifacts}"

mode="${1:-replay}"

# Apple clang ships no libFuzzer runtime, so on macOS the compiler comes from
# Homebrew LLVM. Elsewhere whatever clang++ is on PATH already has it.
compiler="${CXX:-clang++}"

if [[ "$(uname -s)" == "Darwin" && -x /opt/homebrew/opt/llvm/bin/clang++ ]]; then
  compiler=/opt/homebrew/opt/llvm/bin/clang++
fi

if [[ ! -x "$build_dir/fuzz_markdown" ]]; then
  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBLOGIN_FUZZ=ON -DBLOGIN_SANITIZE=ON -DCMAKE_CXX_COMPILER="$compiler"
fi

cmake --build "$build_dir" -j

mkdir -p "$artifacts"

targets_from() {
  if [[ $# -gt 0 ]]; then
    printf '%s\n' "$@"
    return
  fi

  for binary in "$build_dir"/fuzz_*; do
    basename "$binary" | sed 's/^fuzz_//'
  done
}

# A target with a dictionary gets it. One without runs fine on its own, so a
# missing file is not an error.
dictionary_for() {
  local target="$1"

  if [[ -f "$dict_root/$target.dict" ]]; then
    printf -- '-dict=%s' "$dict_root/$target.dict"
  fi
}

# Every target's corpus is capped by libFuzzer's own limits rather than by
# anything here, so nothing is silently dropped from a run.
common=(-artifact_prefix="$artifacts/" -print_final_stats=1 -rss_limit_mb=4096 -timeout=25)

case "$mode" in
  replay)
    shift || true

    failed=0

    for target in $(targets_from "$@"); do
      directories=()

      for candidate in "$corpus_root/$target" "$seed_root/$target" "$regression_root/$target"; do
        if [[ -d "$candidate" ]]; then
          directories+=("$candidate")
        fi
      done

      if [[ ${#directories[@]} -eq 0 ]]; then
        echo "no corpus for $target, skipping" >&2
        continue
      fi

      count=$(find "${directories[@]}" -type f | wc -l | tr -d ' ')
      echo "==> $target: replaying $count inputs"

      # -runs=0 executes each file once and mutates nothing.
      if ! "$build_dir/fuzz_$target" "${common[@]}" -runs=0 "${directories[@]}"; then
        failed=1
      fi
    done

    exit "$failed"
    ;;

  explore)
    shift
    seconds="${1:-60}"
    shift || true

    for target in $(targets_from "$@"); do
      corpus="$corpus_root/$target"
      mkdir -p "$corpus"

      echo "==> $target: exploring for ${seconds}s"

      seeds=("$corpus")

      for extra in "$seed_root/$target" "$regression_root/$target"; do
        if [[ -d "$extra" ]]; then
          seeds+=("$extra")
        fi
      done

      # New inputs are written to the first directory named, so finds land in
      # the corpus and the regression set stays exactly what was put there.
      # shellcheck disable=SC2046
      "$build_dir/fuzz_$target" "${common[@]}" $(dictionary_for "$target") \
        -max_total_time="$seconds" "${seeds[@]}"
    done
    ;;

  minimize)
    shift || true

    for target in $(targets_from "$@"); do
      corpus="$corpus_root/$target"
      merged="$(mktemp -d)"

      echo "==> $target: minimizing"

      "$build_dir/fuzz_$target" -merge=1 "$merged" "$corpus"

      rm -f "$corpus"/*
      cp "$merged"/* "$corpus"/ 2>/dev/null || true
      rm -rf "$merged"

      echo "    $(find "$corpus" -type f | wc -l | tr -d ' ') inputs kept"
    done
    ;;

  *)
    echo "unknown mode: $mode (expected replay, explore, or minimize)" >&2
    exit 2
    ;;
esac
