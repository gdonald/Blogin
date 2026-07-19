#include "data.h"
#include "files.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>
#include <vector>

#include "counters.h"
#include "text.h"
#include "yaml.h"

namespace blogin::data {
namespace {

constexpr std::array data_file_names{"_data.json", "_data.yaml", "_data.yml"};

std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  if (!input) {
    return {};
  }

  count(Counter::files_read);

  return files::read_file(path);
}

}  // namespace

bool is_data_file(const std::filesystem::path& path) {
  const std::string extension = text::to_lower_ascii(path.extension().string());

  return extension == ".json" || extension == ".yaml" || extension == ".yml";
}

std::expected<Value, ParseError> load_file(const std::filesystem::path& path) {
  if (!is_data_file(path)) {
    return Value();
  }

  const std::string text = read(path);
  const std::string extension = text::to_lower_ascii(path.extension().string());

  std::expected<Value, ParseError> parsed =
    extension == ".json" ? parse_json(text) : parse_yaml(text);

  if (!parsed) {
    ParseError error = parsed.error();
    error.message = path.filename().string() + ": " + error.message;

    return std::unexpected(std::move(error));
  }

  return parsed;
}

std::expected<Value, ParseError> load_tree(const std::filesystem::path& directory) {
  Value tree = Value::object();

  std::error_code error;

  if (!std::filesystem::is_directory(directory, error)) {
    return tree;
  }

  std::vector<std::filesystem::directory_entry> entries;

  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    entries.push_back(entry);
  }

  std::sort(entries.begin(), entries.end(),
            [](const auto& left, const auto& right) { return left.path() < right.path(); });

  for (const auto& entry : entries) {
    if (entry.is_symlink()) {
      continue;
    }

    if (entry.is_directory()) {
      std::expected<Value, ParseError> nested = load_tree(entry.path());

      if (!nested) {
        return nested;
      }

      tree.set(entry.path().filename().string(), std::move(*nested));
      continue;
    }

    if (!entry.is_regular_file() || !is_data_file(entry.path())) {
      continue;
    }

    std::expected<Value, ParseError> parsed = load_file(entry.path());

    if (!parsed) {
      return parsed;
    }

    tree.set(entry.path().stem().string(), std::move(*parsed));
  }

  return tree;
}

std::expected<Value, ParseError> resolve(const Value& global, const std::filesystem::path& content,
                                         std::string_view section) {
  Value merged = global;

  std::vector<std::filesystem::path> directories{content};
  std::filesystem::path walk = content;

  for (const std::string_view segment : text::split(section, '/')) {
    if (segment.empty()) {
      continue;
    }

    walk /= segment;
    directories.push_back(walk);
  }

  for (const std::filesystem::path& directory : directories) {
    for (const char* name : data_file_names) {
      const std::filesystem::path path = directory / name;

      std::error_code error;

      if (!std::filesystem::is_regular_file(path, error)) {
        continue;
      }

      std::expected<Value, ParseError> parsed = load_file(path);

      if (!parsed) {
        return parsed;
      }

      if (parsed->is_object()) {
        merged = Value::deep_merge(merged, *parsed);
      }
    }
  }

  return merged;
}

}  // namespace blogin::data
