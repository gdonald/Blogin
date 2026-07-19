#pragma once

#include <cstdio>
#include <cstdlib>

// The fuzz targets are built optimized, so assert compiles away. Aborting is
// what libFuzzer records as a finding, and it writes the input that caused it.
//
// An invariant here says more than "did not crash". A target that only parses
// finds the inputs that segfault. A target that also checks what came back
// finds the inputs that produce wrong output, which is the larger set.
#define FUZZ_REQUIRE(condition)                                                              \
  do {                                                                                       \
    if (!(condition)) {                                                                       \
      std::fprintf(stderr, "fuzz invariant failed: %s\n  at %s:%d\n", #condition, __FILE__,   \
                   __LINE__);                                                                 \
      std::abort();                                                                           \
    }                                                                                         \
  } while (false)
