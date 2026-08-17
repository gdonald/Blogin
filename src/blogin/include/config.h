#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "json.h"
#include "value.h"

namespace blogin {

struct SectionConfig {
  std::string name;
  std::optional<int> page_size;
  std::optional<std::string> label;
  std::optional<int> order;
  std::optional<bool> nav;
  std::optional<std::string> layout;
  std::optional<bool> index_dates;
  std::optional<bool> show_dates;
};

struct LanguageConfig {
  std::string code;
  std::optional<std::string> title;
};

// Everything blogin.json can say. A key with the wrong type is an error rather
// than a silently ignored line, because a setting that does nothing is worse
// than one that complains.
struct Config {
  std::string title;
  std::string base_url;
  std::string output_dir = "public";
  std::string author;
  std::string home_section;
  std::string css_framework = "none";
  std::string theme;

  int page_size = 10;
  int summary_length = 200;
  int reading_wpm = 200;
  int related_count = 5;
  int search_text_length = 2000;
  int search_cap = 10;

  bool clean_urls = false;
  bool debug = false;
  bool search = true;
  bool highlight = false;
  bool robots = true;
  bool minify = false;
  bool fingerprint = false;

  std::vector<int> image_widths;
  std::vector<std::string> taxonomies{"tags"};
  std::vector<std::string> feed_formats{"atom"};
  std::vector<std::string> languages;
  std::vector<LanguageConfig> language_config;
  std::vector<SectionConfig> sections;

  // Keys blogin.json carried that mean nothing here. Reported so a typo is
  // visible.
  std::vector<std::string> unknown_keys;

  const SectionConfig* section(std::string_view name) const;

  static std::expected<Config, ParseError> from_value(const Value& value);

  // A missing file yields the defaults, so an unconfigured site
  // should build with.
  static std::expected<Config, ParseError> load(const std::filesystem::path& path);
};

// "did you mean 'page-size'?" for a key close to a real one, empty otherwise.
std::string nearest_key_hint(std::string_view unknown);

}  // namespace blogin
