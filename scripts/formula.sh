#!/usr/bin/env bash
#
# Write the Homebrew formula for a release, filled in from the artifacts in
# dist/ and their checksums.
#
#   scripts/release.sh all
#   scripts/formula.sh gdonald/blogin v0.1.0 > blogin.rb
#
# The result goes in the tap repository, gdonald/homebrew-blogin, as
# Formula/blogin.rb. Nothing here talks to GitHub: it writes a file.
#
# Installing from the tap needs no toolchain, since it downloads the binary this
# project already built. `brew install --HEAD` builds from source instead, which
# needs only CMake and a compiler because there are no third-party
# dependencies.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

if [[ $# -ne 2 ]]; then
  cat >&2 <<'TEXT'
usage: scripts/formula.sh <owner/repo> <tag>

  owner/repo   where the release lives, for example gdonald/blogin
  tag          the release tag, for example v0.1.0
TEXT
  exit 2
fi

slug="$1"
tag="$2"
version="${tag#v}"

download="https://github.com/$slug/releases/download/$tag"

hash_of() {
  local name="$1"
  local file="dist/$name.sha256"

  if [[ ! -f "$file" ]]; then
    echo "missing $file. Run scripts/release.sh all first" >&2
    exit 1
  fi

  awk '{print $1}' "$file"
}

macos_hash="$(hash_of blogin-macos-universal)"
linux_x86_hash="$(hash_of blogin-linux-x86_64)"
linux_arm_hash="$(hash_of blogin-linux-arm64)"

cat <<FORMULA
class Blogin < Formula
  desc "Fast static blog generator with no runtime dependencies"
  homepage "https://github.com/$slug"
  version "$version"
  license "MIT"

  on_macos do
    # One binary covers Apple silicon and Intel.
    url "$download/blogin-macos-universal"
    sha256 "$macos_hash"
  end

  on_linux do
    on_intel do
      url "$download/blogin-linux-x86_64"
      sha256 "$linux_x86_hash"
    end

    on_arm do
      url "$download/blogin-linux-arm64"
      sha256 "$linux_arm_hash"
    end
  end

  # Building from source needs a compiler and CMake and nothing else, which is
  # what having no third-party dependencies buys.
  head do
    url "https://github.com/$slug.git", branch: "main"

    depends_on "cmake" => :build
    depends_on "ninja" => :build
  end

  def install
    if build.head?
      system "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", *std_cmake_args
      system "cmake", "--build", "build"
      system "cmake", "--install", "build"
    else
      bin.install Dir["blogin-*"].first => "blogin"
    end
  end

  test do
    system bin/"blogin", "init", testpath/"site"
    system bin/"blogin", "build", "--src", testpath/"site/content"

    assert_path_exists testpath/"site/public/index.html"
  end
end
FORMULA
