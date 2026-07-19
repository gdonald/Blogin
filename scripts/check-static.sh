#!/usr/bin/env bash
#
# Assert that a release binary runs where nothing is installed.
#
# On Linux that means fully static, checked by ldd and then by running it in a
# scratch container with no libraries at all. On macOS full static linking is
# impossible, since Apple ships no static libc, so the check is that nothing
# outside /usr/lib and /System is referenced.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

binary="${1:-build/dist/blogin}"

if [[ ! -x "$binary" ]]; then
  echo "no binary at $binary. Build the dist preset first" >&2
  exit 2
fi

case "$(uname -s)" in
  Linux)
    if ldd "$binary" 2>&1 | grep -qv 'not a dynamic executable'; then
      echo "FAIL: $binary is dynamically linked" >&2
      ldd "$binary" >&2
      exit 1
    fi

    echo "ok: not a dynamic executable"

    # Running it where nothing is installed is the check that matters, and it
    # has to be able to fail. busybox carries its own static shell and no
    # libraries, so a binary that needs a loader or a libc cannot start here.
    # `scratch` cannot be pulled, and asking for it fails in a way that reads as
    # success.
    if command -v docker >/dev/null 2>&1; then
      docker run --rm -v "$(cd "$(dirname "$binary")" && pwd)/$(basename "$binary"):/blogin:ro" \
        busybox:latest /blogin init /tmp/site >/dev/null

      echo "ok: builds a site in a container with nothing installed"
    fi
    ;;

  Darwin)
    # A universal binary gets one header line per architecture, and only the
    # dependency lines are indented, so indentation is what selects them.
    linked=$(otool -L "$binary" | grep $'^\t' | awk '{print $1}' | sort -u)
    bad=$(echo "$linked" | grep -v '^/usr/lib/' | grep -v '^/System/' || true)

    if [[ -n "$bad" ]]; then
      echo "FAIL: $binary links libraries outside the system:" >&2
      echo "$bad" >&2
      exit 1
    fi

    echo "ok: links only system libraries"
    echo "$linked" | sed 's/^/    /'

    if command -v lipo >/dev/null 2>&1; then
      echo "ok: architectures: $(lipo -archs "$binary")"
    fi
    ;;

  *)
    echo "unsupported platform" >&2
    exit 2
    ;;
esac
