#include "summary.h"

#include "text.h"

namespace blogin::summary {

std::string truncate(std::string_view input, std::size_t length, std::string_view ellipsis) {
  if (text::char_length(input) <= length) {
    return std::string(input);
  }

  std::string_view cut = text::trim(text::substr(input, 0, length));

  // Cutting mid-word reads badly, so it backs up to the last space.
  if (const auto space = cut.find_last_of(' '); space != std::string_view::npos) {
    cut = cut.substr(0, space);
  }

  return std::string(text::trim(cut)) + std::string(ellipsis);
}

std::string first_block(std::string_view text_content, std::size_t length) {
  for (const std::string_view line : text::split_lines(text_content)) {
    const std::string_view trimmed = text::trim(line);

    if (!trimmed.empty()) {
      return truncate(trimmed, length);
    }
  }

  return {};
}

std::string choose(std::string_view explicit_summary, std::string_view excerpt, std::string_view text_content,
                   std::size_t length) {
  if (!text::trim(explicit_summary).empty()) {
    return std::string(text::trim(explicit_summary));
  }

  if (!text::trim(excerpt).empty()) {
    return std::string(text::trim(excerpt));
  }

  return first_block(text_content, length);
}

}  // namespace blogin::summary
