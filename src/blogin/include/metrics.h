#pragma once

#include <cstddef>
#include <string_view>

namespace blogin::metrics {

std::size_t word_count(std::string_view text);

// Rounded up, and never zero for a post that has any words at all.
int reading_time(std::size_t words, int words_per_minute = 200);

}  // namespace blogin::metrics
