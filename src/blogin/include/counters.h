#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace blogin {

// Named counters of work done: files read, posts parsed, templates
// compiled, pages rendered. Assertions are made against these rather than
// against elapsed time, which is not reproducible across machines.
//
// Cheap enough to leave on in every build.
enum class Counter : std::size_t {
  files_read,
  files_written,
  posts_parsed,
  templates_compiled,
  pages_rendered,
  directory_walks,

  // One per taxonomy term read while scoring related posts. With the inverted
  // index each of a post's terms is visited once. Scoring every post against
  // every other post would visit one per pair.
  related_terms,
  count,
};

const char* counter_name(Counter counter);

void count(Counter counter, std::uint64_t amount = 1);

std::uint64_t counter_value(Counter counter);

void reset_counters();

std::string counters_report();

}  // namespace blogin
