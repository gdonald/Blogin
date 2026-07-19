#!/usr/bin/env bash
#
# Drive the blogin binary itself under AddressSanitizer and UndefinedBehavior-
# Sanitizer, end to end.
#
# The specs run under the sanitizers already, so what they reach is checked. The
# binary reaches further: argument handling, reading a site off disk, writing a
# tree of files, the incremental second build, and the preview server accepting
# a connection. None of that is exercised by calling library functions, and a
# leak or an overrun on those paths is invisible to the spec run.
#
# A sanitizer report makes the process exit non-zero, which fails the script.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

build_dir="${BLOGIN_SANITIZE_BUILD_DIR:-build/sanitize}"

# Absolute, because each command below runs from inside the site it is building.
binary="$root/$build_dir/blogin"

# Fixed and away from the default, so a leftover process from an earlier run
# shows up as a failure here rather than as a port collision somewhere else.
port="${BLOGIN_SANITIZE_PORT:-8791}"

if [[ ! -x "$binary" ]]; then
  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DBLOGIN_SANITIZE=ON \
    -DCMAKE_CXX_COMPILER="${CXX:-clang++}"
  cmake --build "$build_dir" -j
fi

# Leak checking is a Linux-only part of AddressSanitizer, and asking for it on
# macOS is an error rather than something ignored, so the option is added only
# where it exists. Linux is therefore the platform that gates on leaks.
asan_options="detect_stack_use_after_return=1:strict_string_checks=1"

if [[ "$(uname -s)" == "Linux" ]]; then
  asan_options="detect_leaks=1:$asan_options"
fi

export ASAN_OPTIONS="${ASAN_OPTIONS:-$asan_options}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}"

work="$(mktemp -d)"
server_pid=""

cleanup() {
  if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi

  rm -rf "$work"
}

trap cleanup EXIT

site="$work/site"

# Inside the site, because clean refuses an output directory outside the one it
# is run from, which is the check that stops it removing somebody's home tree.
out="public"

# --src names the content directory and everything else is found beside it, so
# each command runs from the site root the way a person running blogin does.
echo "==> init"
"$binary" init "$site" --framework bootstrap5

echo "==> new post"
(cd "$site" && "$binary" new "A Sanitized Post")

echo "==> build"
(cd "$site" && "$binary" build --out "$out" --drafts --future --verbose)

test -f "$site/$out/index.html" || {
  echo "the build wrote no index.html" >&2
  exit 1
}

# The second build takes the incremental path, which reads the manifest the
# first one wrote and decides what to skip. That code never runs on a cold tree.
echo "==> incremental build"
(cd "$site" && "$binary" build --out "$out" --drafts --future)

echo "==> forced rebuild"
(cd "$site" && "$binary" build --out "$out" --force --jobs 4)

echo "==> clean"
(cd "$site" && "$binary" clean --out "$out")

test -f "$site/$out/index.html" && {
  echo "clean left the output in place" >&2
  exit 1
}

# The scaffolded site is one page. The corpus sites are the real ones, with the
# content and layouts that found most of the bugs the specs cover.
echo "==> a build over the corpus sites"
for corpus in "$root"/specs/corpus/*/; do
  if [[ -f "$corpus/blogin.json" ]]; then
    name="$(basename "$corpus")"

    echo "    $name"
    (cd "$corpus" && "$binary" build --out "$work/corpus-out/$name")
  fi
done

# The server is the one part that needs a running process. One request, then it
# is stopped by the pid this script started, never by pattern.
echo "==> serve"
(cd "$site" && "$binary" build --out "$out")
(cd "$site" && exec "$binary" serve --port "$port") &
server_pid=$!

ready=""

for _ in $(seq 1 50); do
  if curl -fsS --max-time 2 "http://127.0.0.1:$port/" > "$work/served.html" 2>/dev/null; then
    ready=yes
    break
  fi

  sleep 0.2
done

if [[ -z "$ready" ]]; then
  echo "the server never answered on port $port" >&2
  exit 1
fi

echo "    served $(wc -c < "$work/served.html" | tr -d ' ') bytes"

# Paths that must not resolve, asked of the running server rather than of the
# function, so the answer covers the socket path as well as the resolver.
for escape in "/../../etc/passwd" "//etc/passwd" "/%2e%2e/%2e%2e/etc/passwd"; do
  status=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 "http://127.0.0.1:$port$escape" || echo 000)

  if [[ "$status" == "200" ]]; then
    echo "the server served $escape" >&2
    exit 1
  fi

  echo "    $escape -> $status"
done

kill "$server_pid"
wait "$server_pid" 2>/dev/null || true
server_pid=""

echo "ok: the binary ran clean under the sanitizers"
