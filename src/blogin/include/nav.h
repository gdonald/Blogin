#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "config.h"

namespace blogin {

// One section of the site, as the content tree describes it.
struct NavNode {
  std::string name;
  std::string label;
  std::string path;
  std::string url;
  int order = 0;
  std::vector<NavNode> children;
};

namespace nav {

struct Options {
  bool clean_urls = false;
  std::string url_prefix;
};

// The content tree as a menu. Every directory under content/ is a section, in
// configured order and then by name, and a section can hide itself from the
// menu without leaving the tree.
std::vector<NavNode> build_tree(const std::filesystem::path& content, const Config& config,
                               const Options& options = {});

// The node whose path matches, at any depth, or null.
const NavNode* find(const std::vector<NavNode>& nodes, std::string_view path);

// True when `section` is the node's own path or lies beneath it, the test
// marks the current item in a menu.
bool is_current(const NavNode& node, std::string_view section);

}  // namespace nav
}  // namespace blogin
