#include "post.h"
#include "files.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>

#include "counters.h"
#include "text.h"
#include "slug.h"

namespace blogin {
namespace {

constexpr std::array known_keys{
  "title", "date", "slug", "tags", "draft", "description", "summary", "toc", "aliases", "order", "layout",
};

// A value may be quoted to keep leading spaces or a colon. The quotes are not
// part of it.
std::string_view unquote(std::string_view value) {
  if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                            (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }

  return value;
}

// A front matter list is written "a, b, c" or "[a, b, c]". Both are read.
std::vector<std::string> parse_list(std::string_view raw) {
  std::string_view value = text::trim(raw);

  if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
    value = value.substr(1, value.size() - 2);
  }

  std::vector<std::string> items;

  for (const std::string_view piece : text::split(value, ',')) {
    const std::string_view trimmed = text::trim(unquote(text::trim(piece)));

    if (!trimmed.empty()) {
      items.emplace_back(trimmed);
    }
  }

  return items;
}

bool parse_bool(std::string_view raw) {
  return text::to_lower_ascii(text::trim(raw)) == "true";
}

std::optional<double> parse_order(std::string_view raw) {
  const std::string value(text::trim(raw));

  if (value.empty()) {
    return std::nullopt;
  }

  return text::to_double(value);
}

bool is_key_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '-' || character == '_';
}

// The filename without its directory, its extension, or a leading date.
std::string filename_stem(std::string_view filename) {
  std::string_view stem = filename;

  if (const auto slash = stem.find_last_of('/'); slash != std::string_view::npos) {
    stem = stem.substr(slash + 1);
  }

  if (const auto dot = stem.find_last_of('.'); dot != std::string_view::npos && dot > 0) {
    stem = stem.substr(0, dot);
  }

  if (stem.size() > 10 && Date::parse_prefix(stem).has_value()) {
    stem = stem.substr(stem[10] == '-' ? 11 : 10);
  }

  return std::string(stem);
}

struct FrontMatter {
  std::vector<std::pair<std::string, std::string>> fields;
  std::string_view body;
};

FrontMatter split_front_matter(std::string_view source) {
  FrontMatter result;
  result.body = source;

  const std::vector<std::string_view> lines = text::split_lines(source);

  if (lines.empty() || text::trim(lines[0]) != "---") {
    return result;
  }

  std::size_t index = 1;

  while (index < lines.size() && text::trim(lines[index]) != "---") {
    const std::string_view line = lines[index];
    const std::string_view trimmed = text::trim_start(line);

    std::size_t length = 0;

    while (length < trimmed.size() && is_key_character(trimmed[length])) {
      ++length;
    }

    if (length > 0 && length < trimmed.size() && trimmed[length] == ':') {
      result.fields.emplace_back(std::string(trimmed.substr(0, length)),
                                 std::string(text::trim(trimmed.substr(length + 1))));
    }

    ++index;
  }

  if (index >= lines.size()) {
    result.body = {};

    return result;
  }

  // Everything after the closing marker, with the blank lines that follow it
  // dropped.
  const std::string_view rest = lines[index].data() + lines[index].size() < source.data() + source.size()
                                  ? source.substr(static_cast<std::size_t>(lines[index].data() +
                                                                           lines[index].size() - source.data()))
                                  : std::string_view{};

  std::size_t start = 0;

  while (start < rest.size() && (rest[start] == '\n' || rest[start] == '\r')) {
    ++start;
  }

  result.body = rest.substr(start);

  return result;
}

std::string_view field_of(const std::vector<std::pair<std::string, std::string>>& fields,
                          std::string_view name) {
  const auto found = std::find_if(fields.begin(), fields.end(),
                                  [&](const auto& entry) { return entry.first == name; });

  return found == fields.end() ? std::string_view{} : std::string_view(found->second);
}

}  // namespace

std::vector<std::string> Post::terms(std::string_view taxonomy) const {
  if (taxonomy == "tags") {
    return tags;
  }

  return parse_list(meta_value(taxonomy));
}

std::string_view Post::meta_value(std::string_view key) const {
  const auto found = std::find_if(meta.begin(), meta.end(),
                                  [&](const auto& entry) { return entry.first == key; });

  return found == meta.end() ? std::string_view{} : std::string_view(found->second);
}

std::expected<Post, ParseError> Post::parse(std::string_view source, std::string_view filename) {
  const FrontMatter front = split_front_matter(source);

  Post post;
  post.filename = std::string(filename);
  post.body = std::string(front.body);

  const std::string_view raw_title = unquote(field_of(front.fields, "title"));

  post.title = raw_title.empty() ? slug::humanize(filename_stem(filename)) : std::string(raw_title);

  if (post.title.empty()) {
    return std::unexpected(ParseError{std::format("missing title in '{}'", filename), 1, 1});
  }

  if (const std::string_view raw_date = unquote(field_of(front.fields, "date")); !raw_date.empty()) {
    const std::optional<Date> parsed = Date::parse(raw_date);

    if (!parsed) {
      return std::unexpected(
        ParseError{std::format("unparseable date '{}' in '{}'", raw_date, filename), 1, 1});
    }

    post.date = *parsed;
  } else if (const std::optional<Date> from_name = Date::parse_prefix(filename); from_name) {
    // A post with no date in its front matter takes one from its filename.
    post.date = *from_name;
  }

  const std::string_view raw_slug = unquote(field_of(front.fields, "slug"));

  post.slug = raw_slug.empty() ? slug::slugify(post.title) : std::string(raw_slug);

  post.tags = parse_list(field_of(front.fields, "tags"));
  post.aliases = parse_list(field_of(front.fields, "aliases"));
  post.draft = parse_bool(field_of(front.fields, "draft"));
  post.toc = parse_bool(field_of(front.fields, "toc"));
  post.description = std::string(unquote(field_of(front.fields, "description")));
  post.summary = std::string(unquote(field_of(front.fields, "summary")));
  post.layout = std::string(unquote(field_of(front.fields, "layout")));
  post.order = parse_order(unquote(field_of(front.fields, "order")));

  for (const auto& [key, value] : front.fields) {
    const bool known = std::find(known_keys.begin(), known_keys.end(), key) != known_keys.end();

    if (!known) {
      post.meta.emplace_back(key, value);
    }
  }

  return post;
}

std::expected<Post, ParseError> Post::load(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  if (!input) {
    return std::unexpected(ParseError{std::format("cannot read '{}'", path.string()), 1, 1});
  }

  count(Counter::files_read);

  const std::string source = files::read_file(path);

  std::expected<Post, ParseError> post = parse(source, path.filename().string());

  if (post) {
    count(Counter::posts_parsed);
  }

  return post;
}

}  // namespace blogin
