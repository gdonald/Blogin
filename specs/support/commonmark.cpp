#include "support/commonmark.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <string_view>

namespace spec {
namespace {

constexpr std::string_view fence = "````````````````````````````````";

std::string read_spec() {
  const std::filesystem::path path = std::filesystem::path(BLOGIN_SPECS_ROOT) / "commonmark" / "spec.txt";

  std::ifstream input(path, std::ios::binary);

  if (!input) {
    return {};
  }

  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::vector<std::string_view> split_lines(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;

  while (start <= text.size()) {
    const auto newline = text.find('\n', start);

    if (newline == std::string_view::npos) {
      if (start < text.size()) {
        lines.push_back(text.substr(start));
      }

      break;
    }

    lines.push_back(text.substr(start, newline - start));
    start = newline + 1;
  }

  return lines;
}

// The specification writes a literal arrow for a tab so the examples stay
// readable. It means a tab.
std::string unescape(std::string_view text) {
  std::string out;
  out.reserve(text.size());

  std::size_t index = 0;

  while (index < text.size()) {
    if (text.compare(index, 3, "\xe2\x86\x92") == 0) {
      out += '\t';
      index += 3;
      continue;
    }

    out += text[index];
    ++index;
  }

  return out;
}

}  // namespace

std::vector<CommonMarkExample> load_commonmark_examples() {
  static const std::string spec = read_spec();

  std::vector<CommonMarkExample> examples;

  if (spec.empty()) {
    return examples;
  }

  const std::vector<std::string_view> lines = split_lines(spec);

  std::string section = "unsectioned";
  int number = 0;

  std::size_t index = 0;

  while (index < lines.size()) {
    const std::string_view line = lines[index];

    ++index;

    if (line.starts_with("#")) {
      std::size_t level = 0;

      while (level < line.size() && line[level] == '#') {
        ++level;
      }

      if (level < line.size() && line[level] == ' ') {
        section = std::string(line.substr(level + 1));
      }

      continue;
    }

    if (!line.starts_with(fence) || !line.contains("example")) {
      continue;
    }

    CommonMarkExample example;
    example.section = section;
    example.number = ++number;

    std::string markdown;
    std::string html;
    bool in_html = false;

    while (index < lines.size()) {
      const std::string_view body = lines[index];

      ++index;

      if (body.starts_with(fence)) {
        break;
      }

      if (body == ".") {
        in_html = true;
        continue;
      }

      (in_html ? html : markdown) += std::string(body) + "\n";
    }

    example.markdown = unescape(markdown);
    example.html = unescape(html);

    examples.push_back(std::move(example));
  }

  return examples;
}

std::string ConformanceReport::describe() const {
  std::string out;

  for (const Section& section : sections) {
    const double share = section.total > 0
                           ? 100.0 * static_cast<double>(section.passed) / static_cast<double>(section.total)
                           : 0.0;

    out += std::format("  {:<34} {:>3}/{:<3} {:>5.1f}%\n", section.name, section.passed, section.total, share);
  }

  const double share = total > 0 ? 100.0 * static_cast<double>(passed) / static_cast<double>(total) : 0.0;

  out += std::format("  {:<34} {:>3}/{:<3} {:>5.1f}%\n", "TOTAL", passed, total, share);

  return out;
}

}  // namespace spec
