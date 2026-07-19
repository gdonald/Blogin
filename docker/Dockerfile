# Linux development and testing, from a generic Debian image rather than a
# preconfigured one. Nothing is pinned to an old version: the LLVM apt
# repository supplies whatever clang is current, and CMake and Ninja come from
# Debian.

FROM debian:trixie

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
