#!/usr/bin/env bash
#
# Move docker/Dockerfile's pins forward to what is current.
#
#   ./scripts/repin-container.sh
#
# Every package in the container is pinned to an exact version, and the base
# image to a digest. That is what stops the Linux jobs building against
# something different from one day to the next. It also means the build fails
# the day Debian or LLVM removes a pinned version from the archive, which
# happens whenever a security update lands.
#
# This is the fix for that. It builds the container with the pins removed, reads
# the versions apt resolved, and writes them back. Run it when the container
# build fails on a version that no longer exists, or when you want the newer
# packages on purpose.
#
# Nothing here is automatic. The point of a pin is that it changes when somebody
# decides it changes.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

dockerfile="docker/Dockerfile"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

echo "==> resolving the base image digest"

docker pull -q debian:trixie > /dev/null
digest="$(docker image inspect debian:trixie --format '{{index .RepoDigests 0}}')"
digest="${digest#debian@}"

echo "    $digest"

# An unpinned copy, so apt picks whatever is current rather than failing on a
# version that has gone.
sed -e 's/^FROM debian@sha256:.*/FROM debian:trixie/' \
    -e 's/=\${LLVM_PACKAGE_VERSION}//g' \
    -e 's/^\( *\)\([a-z0-9+.-]*\)=[^ \\]*\( *\\\)$/\1\2\3/' \
    "$dockerfile" > "$work/Dockerfile"

echo "==> building unpinned, to see what apt resolves"

docker build -q -t blogin-repin -f "$work/Dockerfile" docker > /dev/null

debian_packages=(ca-certificates cmake curl g++ git gnupg imagemagick make ninja-build python3)

llvm_version="$(sed -n 's/^ARG LLVM_VERSION=\(.*\)/\1/p' "$dockerfile")"

echo "==> reading versions"

versions="$(docker run --rm blogin-repin dpkg-query -W -f='${Package}=${Version}\n' \
  "${debian_packages[@]}" "clang-${llvm_version}")"

llvm_package_version="$(echo "$versions" | sed -n "s/^clang-${llvm_version}=//p")"

# Written into a copy and moved over, so a failure part way through leaves the
# Dockerfile as it was.
cp "$dockerfile" "$work/updated"

sed -i.bak "s|^FROM debian@sha256:.*|FROM debian@${digest}|" "$work/updated"
sed -i.bak "s|^ARG LLVM_PACKAGE_VERSION=.*|ARG LLVM_PACKAGE_VERSION=${llvm_package_version}|" "$work/updated"

while IFS= read -r line; do
  package="${line%%=*}"
  version="${line#*=}"

  [[ "$package" == "clang-${llvm_version}" ]] && continue

  sed -i.bak "s|^\( *\)${package}=[^ \\\\]*\( *\\\\\)$|\1${package}=${version}\2|" "$work/updated"
done <<< "$versions"

if diff -q "$dockerfile" "$work/updated" > /dev/null; then
  echo "==> already current, nothing changed"
  exit 0
fi

diff -u "$dockerfile" "$work/updated" || true

cp "$work/updated" "$dockerfile"

echo
echo "==> updated $dockerfile"
echo "    rebuild and test with: ./scripts/test.sh linux gcc tidy coverage-linux"
