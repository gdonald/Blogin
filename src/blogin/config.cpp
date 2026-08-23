#include "config.h"
#include "files.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>

#include "text.h"

namespace blogin {
namespace {

constexpr std::array known_keys{
  "title", "base-url", "output-dir", "author", "home-section", "css-framework", "theme",
  "page-size", "summary-length", "reading-wpm", "related-count", "search-text-length", "search-cap",
  "clean-urls", "debug", "search", "highlight", "robots", "minify", "fingerprint",
  "image-widths", "taxonomies", "feed-formats", "languages", "language-config", "sections",
};

constexpr std::array known_feed_formats{"atom", "rss", "json"};

std::unexpected<ParseError> wrong_type(std::string_view key, std::string_view expected, const Value& value) {
  return std::unexpected(ParseError{
    std::format("config key '{}' must be {}, not {}", key, expected, type_name(value.type())), 1, 1});
}

std::expected<std::string, ParseError> want_string(const Value& value, std::string_view key) {
  if (!value.is_string()) {
    return std::unexpected(wrong_type(key, "a string", value).error());
  }

  return std::string(value.as_string());
}

std::expected<int, ParseError> want_int(const Value& value, std::string_view key) {
  if (!value.is_integer()) {
    return std::unexpected(wrong_type(key, "an integer", value).error());
  }

  return static_cast<int>(value.as_integer());
}

std::expected<bool, ParseError> want_bool(const Value& value, std::string_view key) {
  if (!value.is_boolean()) {
    return std::unexpected(wrong_type(key, "a boolean", value).error());
  }

  return value.as_boolean();
}

std::expected<std::vector<std::string>, ParseError> want_string_list(const Value& value, std::string_view key) {
  if (!value.is_array()) {
    return std::unexpected(wrong_type(key, "a list of strings", value).error());
  }

  std::vector<std::string> items;

  for (const Value& item : value.items()) {
    if (!item.is_string()) {
      return std::unexpected(wrong_type(key, "a list of strings", item).error());
    }

    items.emplace_back(item.as_string());
  }

  return items;
}

std::expected<std::vector<int>, ParseError> want_int_list(const Value& value, std::string_view key) {
  if (!value.is_array()) {
    return std::unexpected(wrong_type(key, "a list of integers", value).error());
  }

  std::vector<int> items;

  for (const Value& item : value.items()) {
    if (!item.is_integer()) {
      return std::unexpected(wrong_type(key, "a list of integers", item).error());
    }

    items.push_back(static_cast<int>(item.as_integer()));
  }

  return items;
}

std::size_t edit_distance(std::string_view left, std::string_view right) {
  std::vector<std::size_t> previous(right.size() + 1);
  std::vector<std::size_t> current(right.size() + 1);

  for (std::size_t index = 0; index <= right.size(); ++index) {
    previous[index] = index;
  }

  for (std::size_t row = 1; row <= left.size(); ++row) {
    current[0] = row;

    for (std::size_t column = 1; column <= right.size(); ++column) {
      const std::size_t substitution = previous[column - 1] + (left[row - 1] == right[column - 1] ? 0 : 1);

      current[column] = std::min({current[column - 1] + 1, previous[column] + 1, substitution});
    }

    previous = current;
  }

  return previous[right.size()];
}

}  // namespace

std::string nearest_key_hint(std::string_view unknown) {
  std::string_view best;
  std::size_t best_distance = std::string_view::npos;

  for (const char* candidate : known_keys) {
    const std::size_t distance = edit_distance(unknown, candidate);

    if (distance < best_distance) {
      best_distance = distance;
      best = candidate;
    }
  }

  // Far enough away and a suggestion is noise.
  if (best_distance > 3 || best.empty()) {
    return {};
  }

  return std::format("did you mean '{}'?", best);
}

const SectionConfig* Config::section(std::string_view name) const {
  const auto found = std::find_if(sections.begin(), sections.end(),
                                  [&](const SectionConfig& entry) { return entry.name == name; });

  return found == sections.end() ? nullptr : &*found;
}

namespace {

// Every format a feed can be written in, checked against the ones there are.
std::expected<void, ParseError> read_feed_formats(const Value& item, Config& config) {
  auto parsed = want_string_list(item, "feed-formats");

  if (!parsed) {
    return std::unexpected(parsed.error());
  }

  for (const std::string& format : *parsed) {
    const auto* const found = std::find_if(known_feed_formats.begin(), known_feed_formats.end(),
                                           [&](const char* candidate) { return format == candidate; });

    if (found == known_feed_formats.end()) {
      return std::unexpected(ParseError{
        std::format("config key 'feed-formats' has an unknown format '{}' (use atom, rss, or json)", format), 1, 1});
    }
  }

  config.feed_formats = std::move(*parsed);

  return {};
}

// The per-language block, which carries a title for each language code.
std::expected<void, ParseError> read_language_config(const Value& item, Config& config) {
  if (!item.is_object()) {
    return std::unexpected(wrong_type("language-config", "a map", item).error());
  }

  for (const Value::Member& entry : item.members()) {
    if (!entry.second.is_object()) {
      return std::unexpected(
        wrong_type(std::format("language-config.{}", entry.first), "a map", entry.second).error());
    }

    LanguageConfig language;
    language.code = entry.first;

    if (const Value* language_title = entry.second.find("title")) {
      auto parsed = want_string(*language_title, std::format("language-config.{}.title", entry.first));

      if (!parsed) {
        return std::unexpected(parsed.error());
      }

      language.title = std::move(*parsed);
    }

    config.language_config.push_back(std::move(language));
  }

  return {};
}

// One section's fields, each of which overrides the site-wide one.
std::expected<void, ParseError> read_section(const Value::Member& entry, SectionConfig& section) {
  const auto field = [&](const char* field_name) { return std::format("sections.{}.{}", entry.first, field_name); };

#define BLOGIN_SECTION_FIELD(reader, name, member)              \
  if (const Value* found = entry.second.find(name)) {           \
    auto parsed = reader(*found, field(name));                  \
                                                                \
    if (!parsed) {                                              \
      return std::unexpected(parsed.error());                   \
    }                                                           \
                                                                \
    section.member = std::move(*parsed);                        \
  }

  BLOGIN_SECTION_FIELD(want_int, "page-size", page_size)
  BLOGIN_SECTION_FIELD(want_string, "label", label)
  BLOGIN_SECTION_FIELD(want_int, "order", order)
  BLOGIN_SECTION_FIELD(want_bool, "nav", nav)
  BLOGIN_SECTION_FIELD(want_string, "layout", layout)
  BLOGIN_SECTION_FIELD(want_bool, "index-dates", index_dates)
  BLOGIN_SECTION_FIELD(want_bool, "show-dates", show_dates)

#undef BLOGIN_SECTION_FIELD

  return {};
}

// The per-section block, whose fields override the site-wide ones.
std::expected<void, ParseError> read_sections(const Value& item, Config& config) {
  if (!item.is_object()) {
    return std::unexpected(wrong_type("sections", "a map", item).error());
  }

  for (const Value::Member& entry : item.members()) {
    if (!entry.second.is_object()) {
      return std::unexpected(wrong_type(std::format("sections.{}", entry.first), "a map", entry.second).error());
    }

    SectionConfig section;
    section.name = entry.first;

    if (auto read = read_section(entry, section); !read) {
      return std::unexpected(read.error());
    }

    config.sections.push_back(std::move(section));
  }

  return {};
}

}  // namespace

#define BLOGIN_STRING_KEY(name, field)                    \
  if (key == (name)) {                                    \
    auto parsed = want_string(item, key);                 \
    if (!parsed) return std::unexpected(parsed.error());  \
    config.field = std::move(*parsed);                    \
    continue;                                             \
  }

#define BLOGIN_INT_KEY(name, field)                       \
  if (key == (name)) {                                    \
    auto parsed = want_int(item, key);                    \
    if (!parsed) return std::unexpected(parsed.error());  \
    config.field = *parsed;                               \
    continue;                                             \
  }

#define BLOGIN_BOOL_KEY(name, field)                      \
  if (key == (name)) {                                    \
    auto parsed = want_bool(item, key);                   \
    if (!parsed) return std::unexpected(parsed.error());  \
    config.field = *parsed;                               \
    continue;                                             \
  }

#define BLOGIN_STRING_LIST_KEY(name, field)               \
  if (key == (name)) {                                    \
    auto parsed = want_string_list(item, key);            \
    if (!parsed) return std::unexpected(parsed.error());  \
    config.field = std::move(*parsed);                    \
    continue;                                             \
  }

std::expected<Config, ParseError> Config::from_value(const Value& value) {
  if (!value.is_object()) {
    return std::unexpected(ParseError{"config must be an object", 1, 1});
  }

  Config config;

  for (const Value::Member& member : value.members()) {
    const std::string& key = member.first;
    const Value& item = member.second;

    const auto *const is_known = std::find_if(known_keys.begin(), known_keys.end(),
                                       [&](const char* candidate) { return key == candidate; });

    if (is_known == known_keys.end()) {
      config.unknown_keys.push_back(key);
      continue;
    }

    BLOGIN_STRING_KEY("title", title)
    BLOGIN_STRING_KEY("base-url", base_url)
    BLOGIN_STRING_KEY("output-dir", output_dir)
    BLOGIN_STRING_KEY("author", author)
    BLOGIN_STRING_KEY("home-section", home_section)
    BLOGIN_STRING_KEY("css-framework", css_framework)
    BLOGIN_STRING_KEY("theme", theme)

    BLOGIN_INT_KEY("page-size", page_size)
    BLOGIN_INT_KEY("summary-length", summary_length)
    BLOGIN_INT_KEY("reading-wpm", reading_wpm)
    BLOGIN_INT_KEY("related-count", related_count)
    BLOGIN_INT_KEY("search-text-length", search_text_length)
    BLOGIN_INT_KEY("search-cap", search_cap)

    BLOGIN_BOOL_KEY("clean-urls", clean_urls)
    BLOGIN_BOOL_KEY("debug", debug)
    BLOGIN_BOOL_KEY("search", search)
    BLOGIN_BOOL_KEY("highlight", highlight)
    BLOGIN_BOOL_KEY("robots", robots)
    BLOGIN_BOOL_KEY("minify", minify)
    BLOGIN_BOOL_KEY("fingerprint", fingerprint)

    BLOGIN_STRING_LIST_KEY("taxonomies", taxonomies)
    BLOGIN_STRING_LIST_KEY("languages", languages)

    if (key == "image-widths") {
      auto parsed = want_int_list(item, key);

      if (!parsed) {
        return std::unexpected(parsed.error());
      }

      config.image_widths = std::move(*parsed);
      continue;
    }

    if (key == "feed-formats") {
      auto read = read_feed_formats(item, config);

      if (!read) {
        return std::unexpected(read.error());
      }

      continue;
    }

    if (key == "language-config") {
      auto read = read_language_config(item, config);

      if (!read) {
        return std::unexpected(read.error());
      }

      continue;
    }

    if (key == "sections") {
      auto read = read_sections(item, config);

      if (!read) {
        return std::unexpected(read.error());
      }

      continue;
    }
  }

  return config;
}

#undef BLOGIN_STRING_KEY
#undef BLOGIN_INT_KEY
#undef BLOGIN_BOOL_KEY
#undef BLOGIN_STRING_LIST_KEY

std::expected<Config, ParseError> Config::load(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  if (!input) {
    return Config{};
  }

  const std::string text = files::read_file(path);

  std::expected<Value, ParseError> value = parse_json(text);

  if (!value) {
    return std::unexpected(value.error());
  }

  return from_value(*value);
}

}  // namespace blogin
