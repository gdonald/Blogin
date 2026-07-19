#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace blogin {

// Shortcodes a site defines for itself, loaded from shortcodes/<name>.html,
// plus the two built in. A user template of the same name wins.
class ShortcodeRegistry {
public:
  static ShortcodeRegistry load(const std::filesystem::path& directory);

  bool contains(std::string_view name) const;

  // The expansion, or the shortcode's own source escaped, when nothing knows
  // the name. Showing what was written beats emitting nothing.
  std::string expand(std::string_view name, std::string_view arguments, std::string_view source) const;

  std::size_t size() const { return templates_.size(); }

private:
  struct Entry {
    std::string name;
    std::string body;
  };

  std::vector<Entry> templates_;
};

namespace shortcode {

struct Argument {
  std::string name;
  std::string value;
};

// key="value" pairs, in the order written.
std::vector<Argument> parse_arguments(std::string_view raw);

std::string_view argument(const std::vector<Argument>& arguments, std::string_view name);

// Substitutes {{ key }} placeholders, escaping each value.
std::string render_template(std::string_view body, const std::vector<Argument>& arguments);

bool is_builtin(std::string_view name);

std::string expand_builtin(std::string_view name, const std::vector<Argument>& arguments);

}  // namespace shortcode
}  // namespace blogin
