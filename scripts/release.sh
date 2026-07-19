#!/usr/bin/env bash
#
# Build the binaries a release ships, with a checksum beside each one.
#
# Three artifacts cover both platforms:
#
#   blogin-macos-universal   arm64 and x86_64 in one file
#   blogin-linux-x86_64      fully static
#   blogin-linux-arm64       fully static
#
# The macOS binary is built natively. The Linux ones are built in the Debian
# container for the architecture they target, so the toolchain that produces a
# release is the toolchain CI tests with. On a machine of the other
# architecture that needs Docker's emulation, which is slow but correct.
#
# Everything lands in dist/. Nothing here uploads anything: publishing is the
# release workflow's job.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

output="$root/dist"

usage() {
  cat >&2 <<'TEXT'
usage: scripts/release.sh [target ...]

targets:
  macos           the universal macOS binary, built natively
  linux-x86_64    the static Linux binary, built in the container
  linux-arm64     the static Linux binary, built in the container
  host            whatever this machine can build without emulation (default)
  all             every target
TEXT
  exit 2
}

checksum() {
  local file="$1"

  if command -v sha256sum >/dev/null 2>&1; then
    (cd "$(dirname "$file")" && sha256sum "$(basename "$file")" > "$(basename "$file").sha256")
  else
    (cd "$(dirname "$file")" && shasum -a 256 "$(basename "$file")" > "$(basename "$file").sha256")
  fi

  echo "==> $(basename "$file")"
  cat "$file.sha256"
}

build_macos() {
  if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "the macOS binary can only be built on macOS" >&2
    exit 1
  fi

  echo "==> building the universal macOS binary"

  cmake --preset dist
  cmake --build build/dist --target blogin -j

  ./scripts/check-static.sh build/dist/blogin

  install -m 755 build/dist/blogin "$output/blogin-macos-universal"
  strip "$output/blogin-macos-universal"

  # Stripping rewrites the file, so the architectures are read back afterward
  # rather than trusted from before.
  lipo -archs "$output/blogin-macos-universal"

  checksum "$output/blogin-macos-universal"
}

build_linux() {
  local arch="$1"
  local platform

  case "$arch" in
    x86_64) platform="linux/amd64" ;;
    arm64) platform="linux/arm64" ;;
    *) echo "unknown architecture: $arch" >&2; exit 2 ;;
  esac

  if ! docker info >/dev/null 2>&1; then
    echo "Docker is not running, and the Linux binaries are built in the container" >&2
    exit 1
  fi

  echo "==> building the static Linux binary for $arch"

  docker build --platform "$platform" -t "blogin-dev-$arch" docker

  # A build directory of its own per architecture, so two of them can sit side
  # by side and neither picks up the other's cache.
  docker run --rm --platform "$platform" -v "$root:/workspace" -w /workspace \
    "blogin-dev-$arch" bash -euo pipefail -c "
      cmake -S . -B build-linux/dist-$arch \
        -DCMAKE_BUILD_TYPE=Release \
        -DBLOGIN_STATIC=ON \
        -DCMAKE_CXX_COMPILER=clang++
      cmake --build build-linux/dist-$arch --target blogin -j\"\$(nproc)\"
      ./scripts/check-static.sh build-linux/dist-$arch/blogin
      strip build-linux/dist-$arch/blogin
      install -m 755 build-linux/dist-$arch/blogin dist/blogin-linux-$arch
    "

  checksum "$output/blogin-linux-$arch"
}

targets=("${@:-host}")

if [[ " ${targets[*]} " == *" all "* ]]; then
  targets=(macos linux-x86_64 linux-arm64)
fi

if [[ " ${targets[*]} " == *" host "* ]]; then
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) targets=(macos linux-arm64) ;;
    Darwin-x86_64) targets=(macos linux-x86_64) ;;
    Linux-x86_64) targets=(linux-x86_64) ;;
    Linux-aarch64 | Linux-arm64) targets=(linux-arm64) ;;
    *) echo "no release target for $(uname -s)-$(uname -m)" >&2; exit 2 ;;
  esac
fi

mkdir -p "$output"

for target in "${targets[@]}"; do
  case "$target" in
    macos) build_macos ;;
    linux-x86_64) build_linux x86_64 ;;
    linux-arm64) build_linux arm64 ;;
    *) usage ;;
  esac
done

echo
echo "==> dist/"
ls -lh "$output"
