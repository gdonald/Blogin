#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "date.h"
#include "json.h"

namespace blogin {

// One Markdown file: its front matter and the body beneath it.
//
// Front matter is a small key-and-value block, not YAML. A post's header is not
// the place to need anchors and flow mappings, so it is read directly.
struct Post {
  std::string title;
  Date date;
  std::string slug;
  std::string description;
  std::string summary;
  std::string layout;
  std::string body;
  std::string filename;

  std::vector<std::string> tags;
  std::vector<std::string> aliases;

  bool draft = false;
  bool toc = false;

  // Absent unless the front matter set it, which is how a section orders its
  // posts by hand rather than by date.
  std::optional<double> order;

  // Front matter keys that are not one of the known ones, kept so a layout can
  // read whatever a site invents.
  std::vector<std::pair<std::string, std::string>> meta;

  std::string date_string() const { return date.valid() ? date.iso() : std::string{}; }

  // The terms this post carries for a taxonomy. "tags" reads the tag list;
  // anything else reads the meta key of that name.
  std::vector<std::string> terms(std::string_view taxonomy) const;

  std::string_view meta_value(std::string_view key) const;

  // `filename` is what error messages name, and where a date and a slug come
  // from when the front matter does not carry them.
  static std::expected<Post, ParseError> parse(std::string_view source, std::string_view filename = {});

  static std::expected<Post, ParseError> load(const std::filesystem::path& path);
};

}  // namespace blogin
