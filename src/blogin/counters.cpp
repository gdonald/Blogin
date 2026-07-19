#include "counters.h"

#include <array>
#include <format>

namespace blogin {
namespace {

constexpr std::size_t counter_count = static_cast<std::size_t>(Counter::count);

std::array<std::atomic<std::uint64_t>, counter_count>& counters() {
  static std::array<std::atomic<std::uint64_t>, counter_count> values{};

  return values;
}

constexpr std::array<const char*, counter_count> names{
  "files_read",
  "files_written",
  "posts_parsed",
  "templates_compiled",
  "pages_rendered",
  "directory_walks",
  "related_terms",
};

}  // namespace

const char* counter_name(Counter counter) {
  return names[static_cast<std::size_t>(counter)];
}

void count(Counter counter, std::uint64_t amount) {
  counters()[static_cast<std::size_t>(counter)].fetch_add(amount, std::memory_order_relaxed);
}

std::uint64_t counter_value(Counter counter) {
  return counters()[static_cast<std::size_t>(counter)].load(std::memory_order_relaxed);
}

void reset_counters() {
  for (auto& value : counters()) {
    value.store(0, std::memory_order_relaxed);
  }
}

std::string counters_report() {
  std::string report;

  for (std::size_t index = 0; index < counter_count; ++index) {
    report += std::format("{}: {}\n", names[index],
                          counters()[index].load(std::memory_order_relaxed));
  }

  return report;
}

}  // namespace blogin
