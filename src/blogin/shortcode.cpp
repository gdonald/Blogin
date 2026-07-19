#include "shortcode.h"
#include "files.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>

#include "counters.h"
#include "text.h"

namespace blogin {
namespace {

void escape_attribute(std::string& out, std::string_view text) {
  for (const char character : text) {
    switch (character) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += character; break;
    }
  }
}

bool is_name_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '-' || character == '_';
}

}  // namespace

namespace shortcode {

std::vector<Argument> parse_arguments(std::string_view raw) {
  std::vector<Argument> arguments;
  std::size_t index = 0;

  while (index < raw.size()) {
    while (index < raw.size() && !is_name_character(raw[index])) {
      ++index;
    }

    const std::size_t name_start = index;

    while (index < raw.size() && is_name_character(raw[index])) {
      ++index;
    }

    if (index == name_start) {
      break;
    }

    const std::string_view name = raw.substr(name_start, index - name_start);

    while (index < raw.size() && (raw[index] == ' ' || raw[index] == '\t')) {
      ++index;
    }

    if (index >= raw.size() || raw[index] != '=') {
      continue;
    }

    ++index;

    while (index < raw.size() && (raw[index] == ' ' || raw[index] == '\t')) {
      ++index;
    }

    if (index >= raw.size() || (raw[index] != '"' && raw[index] != '\'')) {
      continue;
    }

    const char quote = raw[index];
    const std::size_t value_start = ++index;

    while (index < raw.size() && raw[index] != quote) {
      ++index;
    }

    arguments.push_back(Argument{std::string(name), std::string(raw.substr(value_start, index - value_start))});

    if (index < raw.size()) {
      ++index;
    }
  }

  return arguments;
}

std::string_view argument(const std::vector<Argument>& arguments, std::string_view name) {
  const auto found = std::find_if(arguments.begin(), arguments.end(),
                                  [&](const Argument& entry) { return entry.name == name; });

  return found == arguments.end() ? std::string_view{} : std::string_view(found->value);
}

std::string render_template(std::string_view body, const std::vector<Argument>& arguments) {
  std::string out;
  out.reserve(body.size());

  std::size_t index = 0;

  while (index < body.size()) {
    if (body.compare(index, 2, "{{") != 0) {
      out += body[index++];
      continue;
    }

    const auto closing = body.find("}}", index + 2);

    if (closing == std::string_view::npos) {
      out += body[index++];
      continue;
    }

    const std::string_view name = text::trim(body.substr(index + 2, closing - index - 2));

    escape_attribute(out, argument(arguments, name));
    index = closing + 2;
  }

  return out;
}

bool is_builtin(std::string_view name) {
  return name == "youtube" || name == "figure";
}

std::string expand_builtin(std::string_view name, const std::vector<Argument>& arguments) {
  std::string out;

  if (name == "youtube") {
    out += R"(<div class="video"><iframe src="https://www.youtube.com/embed/)";
    escape_attribute(out, argument(arguments, "id"));
    out += "\" allowfullscreen></iframe></div>";

    return out;
  }

  if (name == "figure") {
    out += "<figure><img src=\"";
    escape_attribute(out, argument(arguments, "src"));
    out += "\" alt=\"";
    escape_attribute(out, argument(arguments, "alt"));
    out += "\" />";

    if (const std::string_view caption = argument(arguments, "caption"); !caption.empty()) {
      out += "<figcaption>";
      escape_attribute(out, caption);
      out += "</figcaption>";
    }

    out += "</figure>";
  }

  return out;
}

}  // namespace shortcode

ShortcodeRegistry ShortcodeRegistry::load(const std::filesystem::path& directory) {
  ShortcodeRegistry registry;

  std::error_code error;

  if (!std::filesystem::is_directory(directory, error)) {
    return registry;
  }

  std::vector<std::filesystem::path> files;

  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (entry.is_regular_file() && text::to_lower_ascii(entry.path().extension().string()) == ".html") {
      files.push_back(entry.path());
    }
  }

  std::sort(files.begin(), files.end());

  for (const std::filesystem::path& path : files) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
      continue;
    }

    count(Counter::files_read);

    registry.templates_.push_back(
      Entry{path.stem().string(), files::read_file(path)});
  }

  return registry;
}

bool ShortcodeRegistry::contains(std::string_view name) const {
  const auto found = std::find_if(templates_.begin(), templates_.end(),
                                  [&](const Entry& entry) { return entry.name == name; });

  return found != templates_.end() || shortcode::is_builtin(name);
}

std::string ShortcodeRegistry::expand(std::string_view name, std::string_view arguments,
                                      std::string_view source) const {
  const std::vector<shortcode::Argument> parsed = shortcode::parse_arguments(arguments);

  const auto found = std::find_if(templates_.begin(), templates_.end(),
                                  [&](const Entry& entry) { return entry.name == name; });

  if (found != templates_.end()) {
    return shortcode::render_template(found->body, parsed);
  }

  if (shortcode::is_builtin(name)) {
    return shortcode::expand_builtin(name, parsed);
  }

  std::string out;
  escape_attribute(out, source);

  return out;
}

}  // namespace blogin
