#include "nav.h"

#include <algorithm>
#include <system_error>

#include "slug.h"

namespace blogin::nav {
namespace {

std::string join_path(std::string_view prefix, std::string_view name) {
  return prefix.empty() ? std::string(name) : std::string(prefix) + "/" + std::string(name);
}

std::string url_for(std::string_view path, const Options& options) {
  std::string url = options.url_prefix;
  url += '/';
  url += path;

  if (!options.clean_urls) {
    url += '/';
  }

  return url;
}

std::vector<NavNode> nodes_in(const std::filesystem::path& directory, std::string_view prefix,
                              const Config& config, const Options& options) {
  std::vector<NavNode> nodes;

  std::error_code error;

  if (!std::filesystem::is_directory(directory, error)) {
    return nodes;
  }

  std::vector<std::filesystem::path> children;

  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (entry.is_directory() && !entry.is_symlink()) {
      children.push_back(entry.path());
    }
  }

  std::sort(children.begin(), children.end());

  for (const std::filesystem::path& child : children) {
    const std::string name = child.filename().string();
    const std::string path = join_path(prefix, name);
    const SectionConfig* section = config.section(path);

    // A section can stay in the tree while keeping out of the menu.
    if (section != nullptr && section->nav.has_value() && !*section->nav) {
      continue;
    }

    NavNode node;
    node.name = name;
    node.path = path;
    node.label = section != nullptr && section->label.has_value() ? *section->label : slug::humanize(name);
    node.order = section != nullptr && section->order.has_value() ? *section->order : 0;
    node.url = url_for(path, options);
    node.children = nodes_in(child, path, config, options);

    nodes.push_back(std::move(node));
  }

  std::stable_sort(nodes.begin(), nodes.end(), [](const NavNode& left, const NavNode& right) {
    return left.order != right.order ? left.order < right.order : left.name < right.name;
  });

  return nodes;
}

}  // namespace

std::vector<NavNode> build_tree(const std::filesystem::path& content, const Config& config,
                               const Options& options) {
  return nodes_in(content, {}, config, options);
}

const NavNode* find(const std::vector<NavNode>& nodes, std::string_view path) {
  for (const NavNode& node : nodes) {
    if (node.path == path) {
      return &node;
    }

    if (const NavNode* found = find(node.children, path)) {
      return found;
    }
  }

  return nullptr;
}

bool is_current(const NavNode& node, std::string_view section) {
  return section == node.path || (section.starts_with(node.path) && section.size() > node.path.size() &&
                                  section[node.path.size()] == '/');
}

}  // namespace blogin::nav
