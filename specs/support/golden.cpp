#include "support/golden.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <vector>

#include "support/spec.h"

namespace spec {
namespace {

std::filesystem::path golden_path(std::string_view name) {
  return std::filesystem::path(BLOGIN_SPECS_ROOT) / "golden" / name;
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

}  // namespace

bool regenerating_goldens() {
  const char* value = std::getenv("BLOGIN_REGOLD");

  return value != nullptr && *value != '\0';
}

// A line-oriented diff. Enough to see what moved, without a diff library.
std::string unified_diff(std::string_view expected, std::string_view actual) {
  const std::vector<std::string_view> expected_lines = split_lines(expected);
  const std::vector<std::string_view> actual_lines = split_lines(actual);

  std::string out;
  const std::size_t count = std::max(expected_lines.size(), actual_lines.size());

  for (std::size_t index = 0; index < count; ++index) {
    const std::string_view expected_line = index < expected_lines.size() ? expected_lines[index] : std::string_view{};
    const std::string_view actual_line = index < actual_lines.size() ? actual_lines[index] : std::string_view{};

    if (expected_line == actual_line) {
      continue;
    }

    if (index < expected_lines.size()) {
      out += std::format("  -{}: {}\n", index + 1, expected_line);
    }

    if (index < actual_lines.size()) {
      out += std::format("  +{}: {}\n", index + 1, actual_line);
    }
  }

  return out;
}

void expect_golden(std::string_view name, std::string_view actual, std::source_location where) {
  const std::filesystem::path path = golden_path(name);

  if (regenerating_goldens()) {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(actual.data(), static_cast<std::streamsize>(actual.size()));

    return;
  }

  std::ifstream input(path, std::ios::binary);

  if (!input) {
    record_failure(std::format("no golden file at specs/golden/{}\n"
                               "       run scripts/regold.sh to create it, then review the diff",
                               name),
                   where);
    return;
  }

  const std::string expected((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

  if (expected == actual) {
    return;
  }

  record_failure(std::format("golden mismatch: specs/golden/{}\n{}", name, unified_diff(expected, actual)), where);
}

}  // namespace spec
