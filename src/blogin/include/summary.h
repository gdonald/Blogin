#pragma once

#include <string>
#include <string_view>

namespace blogin::summary {

// The marker that cuts a post's excerpt from the rest of its body.
inline constexpr std::string_view more_marker = "<!--more-->";

// Truncated on a word boundary, with an ellipsis, counting characters rather
// than bytes.
std::string truncate(std::string_view input, std::size_t length, std::string_view ellipsis = "…");

// The first non-empty block of stripped text, capped.
std::string first_block(std::string_view text, std::size_t length);

// An explicit summary wins, then the text before the marker, then the capped
// opening block.
std::string choose(std::string_view explicit_summary, std::string_view excerpt, std::string_view text,
                   std::size_t length = 200);

}  // namespace blogin::summary
