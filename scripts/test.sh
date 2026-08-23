#!/usr/bin/env bash
#
# Everything CI checks, except fuzzing. Run it before a commit and a green run
# here means a green run there.
#
#   ./scripts/test.sh              every stage, several at a time
#   ./scripts/test.sh specs tidy   named stages
#   ./scripts/test.sh native       only what needs no container
#   ./scripts/test.sh -j 14        spend 14 cores across the stages
#   ./scripts/test.sh --serial     one at a time, which is what to read on a failure
#   ./scripts/test.sh --list       the stages and what each one does
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
logs=

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

# Stages run concurrently, each with a share of the cores. Every stage has its
# own build directory, and the spec runner keeps its scratch tree under its own
# pid, so two of them running at once cannot meet.
#
# -j is the whole machine's budget, the way it reads for make and cmake, not a
# count of stages. Four cores are left to the OS, the editor, and the Docker VM,
# since handing over every core makes the machine unusable and buys nothing.
# Past about eight concurrent stages the wall time stops falling.
cores_available="$(jobs_count)"

if [[ $cores_available -ge 12 ]]; then
  default_cores=$((cores_available - 4))
else
  default_cores=$cores_available
fi

# Both set per run, once the budget is known.
jobs=1
parallel=1

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

# Built once per run. Four container stages want it, and building the same tag
# from several processes at once races on the layer cache.
build_image() {
  if [[ "$image_built" == "yes" ]]; then
    return
  fi

  docker build -t "$image" docker
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
  printf '  %-15s %s\n' "-j N" "cores to use across every stage (default $default_cores of $cores_available)"
  printf '  %-15s %s\n' "--serial" "one at a time"
  printf '\nOutput is shown for a stage that fails and swallowed for one that\n'
  printf 'passes. Fuzzing is not here. Run ./scripts/fuzz.sh for that.\n'
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

needs_docker() {
  [[ "$(requirement "$1")" == "docker" ]]
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
  local serial=no

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
      --serial)
        serial=yes
        shift
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

  # Past the default budget the wall time stops falling and the machine becomes
  # unusable while the run goes. Asking for more is capped rather than refused.
  if [[ $cores -gt $default_cores ]]; then
    printf 'capping -j %s at %s, which leaves the machine usable and runs no\n' \
      "$cores" "$default_cores"
    printf 'slower: past that the wall time stops falling.\n\n'
    cores=$default_cores
  fi

  # Two cores per stage is the smallest share that still lets a build overlap
  # its own linking, so the budget decides how many stages run at once.
  parallel=$((cores / 2))

  if [[ $parallel -lt 1 ]]; then
    parallel=1
  fi

  # `wait -n` arrived in bash 4.3. Without it a stage that finishes early cannot
  # hand its slot to the next one, so an old bash runs them one at a time rather
  # than in wrong-sized batches.
  if [[ "${BASH_VERSINFO[0]}" -lt 5 && "${BASH_VERSINFO[0]}${BASH_VERSINFO[1]}" -lt 43 ]]; then
    serial=yes
  fi

  if [[ "$serial" == "yes" ]]; then
    parallel=1
  fi

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

  # Never more stages than there are to run, so a short list gets the whole
  # budget rather than splitting it among slots that stay empty.
  if [[ $parallel -gt ${#selected[@]} ]]; then
    parallel=${#selected[@]}
  fi

  jobs=$(( cores / parallel ))

  if [[ $jobs -lt 1 ]]; then
    jobs=1
  fi

  # Global rather than local, since the EXIT trap fires after main returns and
  # would otherwise read an unbound name.
  logs="$(mktemp -d)"
  trap 'rm -rf "$logs"' EXIT

  local -a passed=() skipped=() failed=() to_run=()

  for name in "${selected[@]}"; do
    case "$(requirement "$name")" in
      docker)
        if [[ "$docker_available" == "no" ]]; then
          printf '    %-16s skipped, Docker is not running\n' "$name"
          skipped+=("$name")

          continue
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

    to_run+=("$name")
  done

  # One image build up front rather than four racing inside the stages.
  local wants_container=no

  for name in "${to_run[@]}"; do
    if needs_docker "$name"; then
      wants_container=yes
    fi
  done

  if [[ "$wants_container" == "yes" ]]; then
    printf '    %-16s building the container image\n' "docker"
    build_image >"$logs/image.log" 2>&1 || {
      printf '    %-16s FAILED\n' "docker image"
      cat "$logs/image.log"
      exit 1
    }
  fi

  local started_at=$SECONDS
  local running=0

  printf '\n%s stage(s), %s at a time, %s of %s cores each\n\n' \
    "${#to_run[@]}" "$parallel" "$jobs" "$cores"

  for name in "${to_run[@]}"; do
    while [[ $running -ge $parallel ]]; do
      wait -n || true
      running=$((running - 1))
    done

    printf '    %-16s started\n' "$name"

    (
      stage_started_at=$SECONDS

      if run_stage "$name" >"$logs/$name.log" 2>&1; then
        printf '    %-16s passed in %ss\n' "$name" "$((SECONDS - stage_started_at))"
      else
        printf '    %-16s FAILED after %ss\n' "$name" "$((SECONDS - stage_started_at))"
        touch "$logs/$name.failed"
      fi
    ) &

    running=$((running + 1))
  done

  wait

  for name in "${to_run[@]}"; do
    if [[ -e "$logs/$name.failed" ]]; then
      failed+=("$name")
    else
      passed+=("$name")
    fi
  done

  # Only a failing stage's output is worth reading. A passing one produced
  # thousands of ok lines nobody looks at.
  for name in "${failed[@]}"; do
    printf '\n================ %s ================\n' "$name"
    cat "$logs/$name.log"
  done

  printf '\n----------------------------------------------------------------\n'
  printf 'passed:  %s\n' "${passed[*]:-none}"

  if [[ ${#skipped[@]} -gt 0 ]]; then
    printf 'skipped: %s\n' "${skipped[*]}"
  fi

  if [[ ${#failed[@]} -gt 0 ]]; then
    printf 'failed:  %s\n' "${failed[*]}"
  fi

  printf 'took:    %ss\n' "$((SECONDS - started_at))"

  if [[ ${#failed[@]} -gt 0 ]]; then
    exit 1
  fi

  # A skipped stage is not a pass. CI runs it, so a commit that goes out on the
  # strength of a run with skips can still fail there.
  if [[ ${#skipped[@]} -gt 0 ]]; then
    printf '\nInstall what those stages want and run them before committing, or\n'
    printf 'name the stages you meant to run to say you left them out.\n'
    exit 1
  fi
}

main "$@"
