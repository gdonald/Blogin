#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "value.h"

namespace blogin::filters {

std::string truncate(std::string_view text, std::size_t length, std::string_view ellipsis = "…");

// A date written as YYYY-MM-DD, formatted with the strftime subset the layouts
// use. Anything that is not a date is passed through unchanged.
std::string format_date(std::string_view iso, std::string_view pattern = "%Y-%m-%d");

struct Group {
  std::string key;
  std::vector<Value> items;
};

// Groups a list of objects by one field, newest key first, as an
// archive listing wants.
std::vector<Group> group_by(const Value& items, std::string_view field);

}  // namespace blogin::filters
