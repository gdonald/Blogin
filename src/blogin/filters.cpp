#include "filters.h"

#include <algorithm>

#include "date.h"
#include "summary.h"

namespace blogin::filters {

std::string truncate(std::string_view text, std::size_t length, std::string_view ellipsis) {
  return summary::truncate(text, length, ellipsis);
}

std::string format_date(std::string_view iso, std::string_view pattern) {
  const std::optional<Date> date = Date::parse(iso);

  if (!date) {
    return std::string(iso);
  }

  return date->format(pattern);
}

std::vector<Group> group_by(const Value& items, std::string_view field) {
  std::vector<Group> groups;

  for (const Value& item : items.items()) {
    const std::string key(item[field].as_string());

    const auto found = std::find_if(groups.begin(), groups.end(),
                                    [&](const Group& group) { return group.key == key; });

    if (found == groups.end()) {
      groups.push_back(Group{key, {item}});
      continue;
    }

    found->items.push_back(item);
  }

  std::sort(groups.begin(), groups.end(),
            [](const Group& left, const Group& right) { return left.key > right.key; });

  return groups;
}

}  // namespace blogin::filters
