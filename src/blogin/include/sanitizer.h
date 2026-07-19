#pragma once

#include <cstddef>

// AddressSanitizer's manual poisoning interface, wrapped so callers do not
// repeat the feature test. Every function here is a no-op in a build without
// the sanitizer.
//
// The sanitizer tracks heap allocations, not what a program carves out of one.
// A bump allocator hands out many nodes from inside a single large block, so
// the sanitizer sees one valid allocation and cannot tell one node from the
// next: a write running eight bytes past a node lands in its neighbour and
// reports nothing. Poisoning the block and unpoisoning each allocation restores
// the distinction.

#ifdef __has_feature
#  if __has_feature(address_sanitizer)
#    define BLOGIN_ASAN 1
#  endif
#endif

#if defined(__SANITIZE_ADDRESS__) && !defined(BLOGIN_ASAN)
#  define BLOGIN_ASAN 1
#endif

#ifdef BLOGIN_ASAN
#  include <sanitizer/asan_interface.h>
#endif

// Marks a function whose arithmetic is meant to wrap or to shift bits off the
// top, which is what a hash function does. The check names are clang's, and
// naming one GCC does not have is an error there rather than something ignored,
// so the attribute is only written for the compiler that understands it.
#ifdef __clang__
#  define BLOGIN_WRAPS_ON_PURPOSE \
    __attribute__((no_sanitize("unsigned-integer-overflow", "unsigned-shift-base")))
#else
#  define BLOGIN_WRAPS_ON_PURPOSE
#endif

namespace blogin {

#ifdef BLOGIN_ASAN
inline constexpr bool sanitizer_enabled = true;
#else
inline constexpr bool sanitizer_enabled = false;
#endif

// Marks bytes unreadable and unwritable. Touching them reports and aborts.
inline void poison_memory([[maybe_unused]] const void* address, [[maybe_unused]] std::size_t size) {
#ifdef BLOGIN_ASAN
  ASAN_POISON_MEMORY_REGION(address, size);
#endif
}

// Marks bytes usable again. Poisoned bytes must be returned to this state
// before the allocator takes the memory back.
inline void unpoison_memory([[maybe_unused]] const void* address, [[maybe_unused]] std::size_t size) {
#ifdef BLOGIN_ASAN
  ASAN_UNPOISON_MEMORY_REGION(address, size);
#endif
}

// Whether a byte is poisoned right now, asked without touching it. Always false
// without the sanitizer, so a caller has to consult sanitizer_enabled to tell
// "readable" from "nothing is tracking poison".
inline bool memory_is_poisoned([[maybe_unused]] const void* address) {
#ifdef BLOGIN_ASAN
  return __asan_address_is_poisoned(address) != 0;
#else
  return false;
#endif
}

}  // namespace blogin
