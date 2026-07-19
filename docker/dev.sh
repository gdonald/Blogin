#!/usr/bin/env bash
#
# Open a shell in the Debian development container with the tree mounted.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="blogin-dev"

docker build -t "$image" "$root/docker"

# The build directory is a container-local volume. Object files and CMake caches
# built against Debian's clang would otherwise collide with the macOS ones in
# the same tree.
docker run --rm -it \
  -v "$root:/workspace" \
  -v blogin-linux-build:/workspace/build-linux \
  -w /workspace \
  "$image" "${@:-bash}"
