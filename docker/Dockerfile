# Linux development and testing, from a generic Debian image rather than a
# preconfigured one. The LLVM apt repository supplies whatever clang is current,
# and CMake and Ninja come from Debian.
#
# The base is pinned by digest rather than by tag, because `debian:trixie` names
# whatever was pushed most recently and a tag can be moved to point at different
# bytes. A digest names the image itself. Dependabot raises a pull request when
# a newer one is published, so this stays current without being trusted blindly.
#
# The packages below are deliberately not version-pinned. Two reasons, and the
# first one is fatal on its own: Debian rebuilds packages per architecture, so
# ninja-build is 1.12.1-1+b1 on arm64 and 1.12.1-1 on amd64. No single version
# is correct for both, and this image is built on each. The second is that a
# security update replaces a version in the same suite, so a pin fails the build
# the day one lands, and nothing updates those pins the way Dependabot updates
# the digest above.

FROM debian@sha256:34cd9e9fd437c0a095ec39cb2e73422c9f30821b0d0848ed74fd0d43bae4d958

ARG LLVM_VERSION=21
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install --no-install-recommends -y \
      ca-certificates \
      cmake \
      curl \
      g++ \
      git \
      gnupg \
      imagemagick \
      make \
      ninja-build \
      python3 \
      valgrind \
    && rm -rf /var/lib/apt/lists/*

# g++ is not what blogin ships with. It is a second opinion: a different
# frontend reports things clang accepts, and building against libstdc++ rather
# than libc++ exercises a different standard library underneath the same code.

# ImageMagick above is for the tests, not for building: it is the resizer the
# responsive-image specs need in order to run rather than report themselves
# pending. Blogin itself calls whichever resizer it finds and works without one.
# Valgrind is what reads of uninitialised memory are caught with. The
# sanitizers do not report them and neither does coverage, so this is the one
# tool in the set that sees that class of defect. Linux only, which is why it
# lives here rather than on the host.

# Debian's own clang lags the LLVM releases, so the apt repository is added and
# preferred. If it ever carries a newer clang than the repository, this still
# installs the newer of the two.
#
# apt.llvm.org goes down for minutes at a time, and when it does the connection
# is refused rather than hanging, so a single attempt fails the build in under a
# second. Both the key fetch and the package fetch retry with a delay between
# attempts. deb.debian.org above needs none of this.
RUN curl -fsSL --retry 5 --retry-delay 10 --retry-all-errors --connect-timeout 15 \
      https://apt.llvm.org/llvm-snapshot.gpg.key \
      | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/trixie/ llvm-toolchain-trixie-${LLVM_VERSION} main" \
      > /etc/apt/sources.list.d/llvm.list \
    && apt-get -o Acquire::Retries=5 update \
    && apt-get -o Acquire::Retries=5 install --no-install-recommends -y \
      clang-${LLVM_VERSION} \
      clang-tidy-${LLVM_VERSION} \
      clang-tools-${LLVM_VERSION} \
      lld-${LLVM_VERSION} \
      llvm-${LLVM_VERSION} \
      libc++-${LLVM_VERSION}-dev \
      libc++abi-${LLVM_VERSION}-dev \
      libclang-rt-${LLVM_VERSION}-dev \
    && rm -rf /var/lib/apt/lists/*

RUN for tool in clang clang++ clang-tidy run-clang-tidy lld llvm-cov llvm-profdata; do \
      ln -sf /usr/bin/${tool}-${LLVM_VERSION} /usr/local/bin/${tool}; \
    done

ENV CC=clang
ENV CXX=clang++
ENV CMAKE_GENERATOR=Ninja

WORKDIR /workspace
