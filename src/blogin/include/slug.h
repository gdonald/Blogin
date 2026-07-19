#pragma once

#include <string>
#include <string_view>

namespace blogin::slug {

// Lowercased, with every run of characters that is not a letter or digit
// replaced by a single hyphen, and no hyphen at either end.
std::string slugify(std::string_view text);

// The inverse reading of a file or directory name: hyphens and underscores
// become spaces and each word is capitalised.
std::string humanize(std::string_view text);

}  // namespace blogin::slug
