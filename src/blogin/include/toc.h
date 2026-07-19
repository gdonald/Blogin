#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace blogin {

// One heading, as the renderer saw it.
struct Heading {
  int level = 0;
  std::string text;
  std::string id;
};

namespace toc {

struct Entry {
  std::string title;
  std::string id;
  int level = 0;
  std::vector<Entry> children;
};

// Flat headings into a tree, nested by level.
std::vector<Entry> build(const std::vector<Heading>& headings);

std::string render(const std::vector<Entry>& entries);

}  // namespace toc
}  // namespace blogin
