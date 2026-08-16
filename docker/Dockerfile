# Linux development and testing, from a generic Debian image rather than a
# preconfigured one. The LLVM apt repository supplies whatever clang is current,
# and CMake and Ninja come from Debian.
#
# The base is pinned by digest rather than by tag, because `debian:trixie` names
# whatever was pushed most recently and a tag can be moved to point at different
# bytes. A digest names the image itself. Dependabot raises a pull request when
# a newer one is published, so this stays current without being trusted blindly.
#
# The packages below are not version-pinned. Debian security updates land inside
# the same suite, so pinning versions would fail the build the day one shipped,
# and nothing updates those pins automatically the way Dependabot updates this
# digest.

FROM debian@sha256:34cd9e9fd437c0a095ec39cb2e73422c9f30821b0d0848ed74fd0d43bae4d958

ARG LLVM_VERSION=21

# The LLVM packages all ship as one version, so it is named once here rather
# than eight times below.
ARG LLVM_PACKAGE_VERSION=1:21.1.8~++20251221033036+2078da43e25a-1~exp1~20251221153213.50

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install --no-install-recommends -y \
      ca-certificates=20250419 \
      cmake=3.31.6-2 \
      curl=8.14.1-2+deb13u4 \
      g++=4:14.2.0-1 \
      git=1:2.47.3-0+deb13u1 \
      gnupg=2.4.7-21+deb13u1 \
      imagemagick=8:7.1.1.43+dfsg1-1+deb13u11 \
      make=4.4.1-2 \
      ninja-build=1.12.1-1+b1 \
      python3=3.13.5-1 \
    && rm -rf /var/lib/apt/lists/*

# g++ is not what blogin ships with. It is a second opinion: a different
# frontend reports things clang accepts, and building against libstdc++ rather
# than libc++ exercises a different standard library underneath the same code.

# ImageMagick above is for the tests, not for building: it is the resizer the
# responsive-image specs need in order to run rather than report themselves
# pending. Blogin itself calls whichever resizer it finds and works without one.

# Debian's own clang lags the LLVM releases, so the apt repository is added and
# preferred. If it ever carries a newer clang than the repository, this still
# installs the newer of the two.
RUN curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key \
      | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/trixie/ llvm-toolchain-trixie-${LLVM_VERSION} main" \
      > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install --no-install-recommends -y \
      clang-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION} \
      clang-tidy-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION} \
      clang-tools-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION} \
      lld-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION} \
      llvm-${LLVM_VERSION}=${LLVM_PACKAGE_VERSION} \
      libc++-${LLVM_VERSION}-dev=${LLVM_PACKAGE_VERSION} \
      libc++abi-${LLVM_VERSION}-dev=${LLVM_PACKAGE_VERSION} \
      libclang-rt-${LLVM_VERSION}-dev=${LLVM_PACKAGE_VERSION} \
    && rm -rf /var/lib/apt/lists/*

RUN for tool in clang clang++ clang-tidy run-clang-tidy lld llvm-cov llvm-profdata; do \
      ln -sf /usr/bin/${tool}-${LLVM_VERSION} /usr/local/bin/${tool}; \
    done

ENV CC=clang
ENV CXX=clang++
ENV CMAKE_GENERATOR=Ninja

WORKDIR /workspace
