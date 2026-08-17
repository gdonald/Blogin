#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace blogin::highlight {

// The languages that get server-side highlighting. A language outside this set
// renders as escaped text with a class saying so.
std::vector<std::string_view> languages();

bool supports(std::string_view language);

// Escaped HTML with keyword, string, number, and comment runs wrapped in spans.
std::string render(std::string_view code, std::string_view language);

}  // namespace blogin::highlight
