#include "metrics.h"

#include "text.h"

namespace blogin::metrics {

std::size_t word_count(std::string_view text) {
  return text::word_count(text);
}

int reading_time(std::size_t words, int words_per_minute) {
  if (words == 0 || words_per_minute <= 0) {
    return 0;
  }

  const auto minutes = static_cast<int>((words + static_cast<std::size_t>(words_per_minute) - 1) /
                                        static_cast<std::size_t>(words_per_minute));

  return minutes < 1 ? 1 : minutes;
}

}  // namespace blogin::metrics
