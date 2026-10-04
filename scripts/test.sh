#!/usr/bin/env bash
#
# Everything CI checks, except fuzzing. Run it before a commit and a green run
# here means a green run there.
#
#   ./scripts/test.sh              every stage
#   ./scripts/test.sh specs tidy   named stages
#   ./scripts/test.sh native       only what needs no container
#   ./scripts/test.sh -j 14        give each stage 14 cores
#   ./scripts/test.sh --list       the stages and what each one does
#
# Stages run one at a time, and the run stops at the first stage that fails.
# That stage's output is printed and also kept under build/test-logs/, so the
# error can be read after it scrolls past.
#
# Each job in .github/workflows/ci.yml calls one stage from this file, so the
# two cannot drift: a stage added here is added there, and a stage that fails
# here fails there for the same reason.
#
# The codeql stage is the exception. Its CI counterpart is a separate workflow,
# .github/workflows/codeql.yml, which runs the CodeQL action rather than this
# file, because uploading to the security tab is the action's job. Both name the
# same query suite, so a finding in one is a finding in the other.
#
# A stage is one build tree and every check that can run against it. Nothing is
# compiled twice for the same instrumentation. The stages do not share a build
# because they cannot: the sanitizers, ThreadSanitizer, and coverage each
# instrument the code differently, and CMakeLists refuses to combine them.
# Sanitizers and the integer checks do combine, so they are one build here, and
# both the specs and the binary run against it.
#
# Fuzzing is deliberately left out. Replay is a second in CI but wants the
# container and the whole corpus, and exploring is a time budget rather than a
# pass or a fail. Run ./scripts/fuzz.sh for that.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

image="blogin-dev"
image_built=no

# The container writes into build-linux/ and never into build/, so a container
# stage cannot clobber the cache a native stage left behind. They share the
# working tree through one mount.
container_build_root="build-linux"

jobs_count() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  else
    sysctl -n hw.ncpu
  fi
}

# -j is the cores each stage gets. Four are left to the OS, the editor, and the
# Docker VM, since handing over every core makes the machine unusable.
cores_available="$(jobs_count)"

if [[ $cores_available -ge 12 ]]; then
  default_cores=$((cores_available - 4))
else
  default_cores=$cores_available
fi

# Set per run, once the budget is known.
jobs=1

# ---------------------------------------------------------------------------
# Stages
# ---------------------------------------------------------------------------

stage_specs() {
  cmake --preset debug
  cmake --build build/debug -j
  ./build/debug/blogin_specs --jobs "$jobs"
}

# The integer preset is the sanitize preset plus three more checks, so this one
# build covers both. Two checks run against it: the specs, which reach library
# functions, and the binary itself, which reaches argument handling, a site on
# disk, the incremental second build, and the preview server.
stage_sanitize() {
  cmake --preset integer
  cmake --build build/integer -j
  ./build/integer/blogin_specs --jobs "$jobs"

  BLOGIN_SANITIZE_BUILD_DIR=build/integer ./scripts/sanitize-cli.sh
}

stage_thread() {
  cmake --preset thread
  cmake --build build/thread -j
  ./build/thread/blogin_specs --jobs "$jobs"
}

stage_dist() {
  cmake --preset dist
  cmake --build build/dist -j
  ./scripts/check-static.sh
}

stage_coverage() {
  ./scripts/coverage.sh
}

# The suite the CodeQL workflow runs, so an alert on the security tab is caught
# before the push that would raise it. This one needs the CodeQL CLI rather than
# the container, and it builds the tree a second time because CodeQL has to
# watch a compile it started itself.
stage_codeql() {
  ./scripts/codeql.sh
}

# ---------------------------------------------------------------------------
# Stages that need the container
# ---------------------------------------------------------------------------

# Built once per run, before the first container stage.
build_image() {
  if [[ "$image_built" == "yes" ]]; then
    return
  fi

  # buildx keeps a build in its own cache unless it is told to load the result
  # into the daemon, and `docker build` is buildx wherever the plugin is
  # installed. Without --load the build reports success and the tag is still
  # missing, which the first `docker run` reports as a denied pull of a public
  # image by that name.
  if docker buildx version >/dev/null 2>&1; then
    docker buildx build --load -t "$image" docker
  else
    docker build -t "$image" docker
  fi

  docker image inspect "$image" >/dev/null

  image_built=yes
}

in_container() {
  docker run --rm \
    -v "$root:/workspace" \
    -v blogin-linux-build:/workspace/$container_build_root \
    -w /workspace \
    -e "BLOGIN_JOBS=$jobs" \
    "$image" bash -euo pipefail -c "$1"
}

stage_linux() {
  ./scripts/test-linux.sh
}

stage_linux_static() {
  build_image
  in_container "
    cmake -S . -B $container_build_root/static -DCMAKE_BUILD_TYPE=Release \
      -DBLOGIN_STATIC=ON -DCMAKE_CXX_COMPILER=clang++
    cmake --build $container_build_root/static -j\"\${BLOGIN_JOBS:-\$(nproc)}\"
    ./scripts/check-static.sh $container_build_root/static/blogin
  "
}

# GCC is supported on Linux, so it gets the same shape of run clang gets: a
# plain build and a sanitizer build, both against libstdc++ rather than libc++.
# A different frontend also reports what clang accepts.
#
# The plain build turns on libstdc++'s debug containers, which is the check for
# an iterator used after its container moved it and for a range whose ends come
# from different containers. It needs libstdc++, so this is the stage that has
# it. The sanitizer build below leaves it off: AddressSanitizer is what that
# build is for, and debug containers on top of it only make it slower.
#
# The integer checks and coverage are left out because they are clang's alone,
# which CMakeLists refuses at configure time rather than mid-build.
stage_gcc() {
  build_image
  in_container "
    cmake -S . -B $container_build_root/gcc -DCMAKE_BUILD_TYPE=Debug -DBLOGIN_GLIBCXX_DEBUG=ON \
      -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc
    cmake --build $container_build_root/gcc -j\"\${BLOGIN_JOBS:-\$(nproc)}\"
    ./$container_build_root/gcc/blogin_specs --jobs \"\${BLOGIN_JOBS:-\$(nproc)}\"

    cmake -S . -B $container_build_root/gcc-asan -DCMAKE_BUILD_TYPE=Debug -DBLOGIN_SANITIZE=ON \
      -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc
    cmake --build $container_build_root/gcc-asan -j\"\${BLOGIN_JOBS:-\$(nproc)}\"
    ./$container_build_root/gcc-asan/blogin_specs --jobs \"\${BLOGIN_JOBS:-\$(nproc)}\"
  "
}

# In the container on every host, because Valgrind does not run on macOS. The
# build carries no sanitizer: memcheck and the sanitizers each replace the
# allocator, and a binary carrying both reports nothing useful.
stage_valgrind() {
  build_image
  in_container "
    BLOGIN_VALGRIND_BUILD_DIR=$container_build_root/valgrind ./scripts/valgrind.sh
  "
}

# In the container on every host, because Apple ships no clang-tidy and a
# different clang-tidy version reports a different set.
stage_tidy() {
  build_image
  in_container "
    cmake -S . -B $container_build_root/tidy -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
    BLOGIN_TIDY_BUILD_DIR=$container_build_root/tidy ./scripts/tidy.sh
  "
}

# The same floor on both platforms. Code reached only on one of them is covered
# there and not here, so a number that holds on both is a number that holds.
stage_coverage_linux() {
  build_image
  in_container "
    export CC=clang CXX=clang++
    BLOGIN_COVERAGE_BUILD_DIR=$container_build_root/coverage ./scripts/coverage.sh

    # The container's build tree is a docker volume rather than a directory in
    # the working tree, so a file left there is not on the host afterwards. The
    # lcov export is copied to the mounted tree, which is where CI reads it.
    # Two edits on the way out.
    #
    # Paths come out absolute and rooted at the container's mount point, which
    # names no file a coverage service can find in the repository. Stripping the
    # prefix leaves them relative to the tree they came from.
    #
    # Branch records are dropped so the published number is the line coverage
    # this script gates on. A service reading them counts a line whose branches
    # were not all taken as partly covered, which is a third number, neither the
    # line coverage nor the branch coverage, and it matches no floor here.
    # Branch coverage is still checked, by coverage.sh, against its own floor.
    mkdir -p build
    sed -e 's|^SF:/workspace/|SF:|' -e '/^BR[DFH]/d' \
      $container_build_root/coverage/coverage.lcov > build/coverage-linux.lcov
  "
}

# ---------------------------------------------------------------------------
# Stage table
# ---------------------------------------------------------------------------

# name|what it needs beyond a compiler|what it does
stages=(
  "specs|nothing|Debug build, then the whole spec suite"
  "sanitize|nothing|Sanitizer build, then the specs and the binary itself"
  "thread|nothing|ThreadSanitizer build, then the specs"
  "dist|nothing|Distribution build, then that it runs with nothing installed"
  "coverage|nothing|Instrumented build, then line and branch coverage against the floor"
  "codeql|codeql|The CodeQL suite the security tab reports, over the whole tree"
  "linux|docker|The Debian container: debug, sanitizers, ThreadSanitizer"
  "linux-static|docker|The static Linux binary, checked with nothing installed"
  "gcc|docker|GCC builds against libstdc++, plain and with sanitizers, then the specs"
  "tidy|docker|clang-tidy over every translation unit"
  "valgrind|docker|The specs under memcheck, for uninitialised reads and leaks"
  "coverage-linux|docker|The same coverage floor on Linux"
)

native_stages=(specs sanitize thread dist coverage codeql)
container_stages=(linux linux-static gcc tidy valgrind coverage-linux)

field() {
  printf '%s' "$1" | cut -d'|' -f"$2"
}

describe_stages() {
  printf 'usage: scripts/test.sh [stage ...]\n\n'
  printf 'stages:\n'

  for entry in "${stages[@]}"; do
    printf '  %-15s %s\n' "$(field "$entry" 1)" "$(field "$entry" 3)"
  done

  printf '\ngroups:\n'
  printf '  %-15s %s\n' "all" "every stage above (the default)"
  printf '  %-15s %s\n' "native" "only the stages that need no container"

  printf '\nA stage whose requirement is missing is skipped and the run fails, since\n'
  printf 'CI runs it either way. Docker is what the container stages want, and the\n'
  printf 'CodeQL CLI is what the codeql stage wants.\n'

  printf '\noptions:\n'
  printf '  %-15s %s\n' "-j N" "cores each stage uses (default $default_cores of $cores_available)"
  printf '\nStages run one at a time and the run stops at the first failure. Output\n'
  printf 'is shown for the stage that failed and swallowed for one that passes.\n'
  printf 'Every stage'"'"'s output is kept in build/test-logs/<stage>.log.\n'
  printf 'Fuzzing is not here. Run ./scripts/fuzz.sh for that.\n'
}

# What a stage needs that a bare checkout does not have: "docker", "codeql", or
# "nothing".
requirement() {
  for entry in "${stages[@]}"; do
    if [[ "$(field "$entry" 1)" == "$1" ]]; then
      field "$entry" 2

      return
    fi
  done

  printf 'nothing'
}

known_stage() {
  for entry in "${stages[@]}"; do
    if [[ "$(field "$entry" 1)" == "$1" ]]; then
      return 0
    fi
  done

  return 1
}

run_stage() {
  local name="$1"

  # A stage name is a shell function with hyphens turned into underscores.
  "stage_${name//-/_}"
}

# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

# Wrapped in a function and called on the last line, so bash reads the whole
# file before running any of it. bash otherwise reads a script incrementally by
# byte offset, and a file rewritten mid-run resumes at a stale offset in the new
# content, failing with a syntax error nowhere near anything wrong.
main() {
  local cores=$default_cores

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --list | -l | --help | -h)
        describe_stages
        exit 0
        ;;
      --jobs | -j)
        cores="${2:-}"
        shift 2 || true
        ;;
      *)
        break
        ;;
    esac
  done

  if ! [[ "$cores" =~ ^[1-9][0-9]*$ ]]; then
    echo "--jobs wants a positive number of cores, not '$cores'" >&2
    exit 2
  fi

  if [[ $cores -gt $default_cores ]]; then
    printf 'capping -j %s at %s, which leaves the machine usable\n\n' "$cores" "$default_cores"
    cores=$default_cores
  fi

  jobs=$cores

  local requested=("${@:-all}")
  local selected=()
  local name

  for name in "${requested[@]}"; do
    case "$name" in
      all) selected+=("${native_stages[@]}" "${container_stages[@]}") ;;
      native) selected+=("${native_stages[@]}") ;;
      *)
        if ! known_stage "$name"; then
          echo "unknown stage '$name'" >&2
          echo >&2
          describe_stages >&2
          exit 2
        fi

        selected+=("$name")
        ;;
    esac
  done

  local docker_available=no
  local codeql_available=no

  if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
    docker_available=yes
  fi

  if command -v codeql >/dev/null 2>&1; then
    codeql_available=yes
  fi

  local log_dir="$root/build/test-logs"
  mkdir -p "$log_dir"

  local -a passed=() skipped=()
  local started_at=$SECONDS

  printf '\n%s stage(s), one at a time, %s cores each\n\n' "${#selected[@]}" "$jobs"

  for name in "${selected[@]}"; do
    case "$(requirement "$name")" in
      docker)
        if [[ "$docker_available" == "no" ]]; then
          printf '    %-16s skipped, Docker is not running\n' "$name"
          skipped+=("$name")

          continue
        fi

        if [[ "$image_built" == "no" ]]; then
          printf '    %-16s building the container image\n' "docker"

          if ! build_image >"$log_dir/image.log" 2>&1; then
            printf '    %-16s FAILED\n\n' "docker image"
            cat "$log_dir/image.log"
            printf '\nlog: %s\n' "$log_dir/image.log"
            exit 1
          fi
        fi
        ;;
      codeql)
        if [[ "$codeql_available" == "no" ]]; then
          printf '    %-16s skipped, no codeql on PATH\n' "$name"
          skipped+=("$name")

          continue
        fi
        ;;
    esac

    local log="$log_dir/$name.log"
    local stage_started_at=$SECONDS

    printf '    %-16s started\n' "$name"

    # Started as a background job and waited on, not called as an if
    # condition: bash ignores set -e inside a condition, so a stage whose build
    # failed would go on to run its specs against the last binary.
    ( run_stage "$name" ) >"$log" 2>&1 &

    if wait "$!"; then
      printf '    %-16s passed in %ss\n' "$name" "$((SECONDS - stage_started_at))"
      passed+=("$name")

      continue
    fi

    printf '    %-16s FAILED after %ss\n' "$name" "$((SECONDS - stage_started_at))"
    printf '\n================ %s ================\n' "$name"
    cat "$log"
    printf '\n----------------------------------------------------------------\n'
    printf 'passed:  %s\n' "${passed[*]:-none}"
    printf 'failed:  %s\n' "$name"
    printf 'log:     %s\n' "$log"
    exit 1
  done

  printf '\n----------------------------------------------------------------\n'
  printf 'passed:  %s\n' "${passed[*]:-none}"

  if [[ ${#skipped[@]} -gt 0 ]]; then
    printf 'skipped: %s\n' "${skipped[*]}"
  fi

  printf 'took:    %ss\n' "$((SECONDS - started_at))"

  # A skipped stage is not a pass. CI runs it, so a commit that goes out on the
  # strength of a run with skips can still fail there.
  if [[ ${#skipped[@]} -gt 0 ]]; then
    printf '\nInstall what those stages want and run them before committing, or\n'
    printf 'name the stages you meant to run to say you left them out.\n'
    exit 1
  fi
}

main "$@"
