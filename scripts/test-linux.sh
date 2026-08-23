#!/usr/bin/env bash
#
# Configure, build, and run the specs on Linux, in the Debian container.
# This is what CI runs for the Linux job, so a green run here means the same
# thing it means there.
#
# Three builds, because three instrumentations. Plain, then the sanitizers with
# the integer checks on top, then ThreadSanitizer, which keeps its own shadow
# memory and cannot share a build with the others. The binary drive runs against
# the sanitizer build rather than building a fourth.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="blogin-dev"

# --load because `docker build` is buildx wherever the plugin is installed, and
# buildx leaves the result in its own cache rather than in the daemon's image
# store unless it is told otherwise.
if docker buildx version >/dev/null 2>&1; then
  docker buildx build --load -t "$image" "$root/docker"
else
  docker build -t "$image" "$root/docker"
fi

docker image inspect "$image" >/dev/null

docker run --rm \
  -e "BLOGIN_JOBS=${BLOGIN_JOBS:-}" \
  -v "$root:/workspace" \
  -v blogin-linux-build:/workspace/build-linux \
  -w /workspace \
  "$image" bash -euo pipefail -c '
    cmake -S . -B build-linux/debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
    cmake --build build-linux/debug -j"${BLOGIN_JOBS:-$(nproc)}"
    ./build-linux/debug/blogin_specs --jobs "${BLOGIN_JOBS:-$(nproc)}"

    cmake -S . -B build-linux/integer -DCMAKE_BUILD_TYPE=Debug -DBLOGIN_SANITIZE=ON \
      -DBLOGIN_SANITIZE_INTEGER=ON -DCMAKE_CXX_COMPILER=clang++
    cmake --build build-linux/integer -j"${BLOGIN_JOBS:-$(nproc)}"
    ./build-linux/integer/blogin_specs --jobs "${BLOGIN_JOBS:-$(nproc)}"

    BLOGIN_SANITIZE_BUILD_DIR=build-linux/integer ./scripts/sanitize-cli.sh

    cmake -S . -B build-linux/tsan -DCMAKE_BUILD_TYPE=Debug -DBLOGIN_SANITIZE_THREAD=ON -DCMAKE_CXX_COMPILER=clang++
    cmake --build build-linux/tsan -j"${BLOGIN_JOBS:-$(nproc)}"
    ./build-linux/tsan/blogin_specs --jobs "${BLOGIN_JOBS:-$(nproc)}"
  '
