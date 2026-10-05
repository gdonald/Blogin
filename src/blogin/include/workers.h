#pragma once

#include <cstddef>
#include <functional>

namespace blogin {

// What a thread started here gets for its stack. macOS gives a new thread
// 512 KiB, Linux gives it 8 MiB, and an expression nested to the evaluator's
// limit needs more than 512 KiB in a sanitizer build. This is the size of a
// main thread's stack on both.
inline constexpr std::size_t worker_stack_bytes = std::size_t{8} * 1024 * 1024;

// Runs `task` on `count` threads at once, or on the calling thread when
// `count` is zero, and returns when every run has finished.
void run_on_workers(unsigned count, const std::function<void()>& task);

}  // namespace blogin
