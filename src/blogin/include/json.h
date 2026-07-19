#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "value.h"

namespace blogin {

struct ParseError {
  std::string message;
  std::size_t line = 1;
  std::size_t column = 1;

  // "line 3, column 12: trailing comma before }"
  std::string describe() const;

  // The same, prefixed with a path, for errors a user reads.
  std::string describe(std::string_view path) const;
};

std::expected<Value, ParseError> parse_json(std::string_view text);

enum class JsonStyle {
  compact,
  pretty,
};

// `sorted_keys` orders object keys by name rather than by insertion. The search
// index uses it so that the emitted file is stable whatever order posts were
// discovered in.
std::string to_json(const Value& value, JsonStyle style = JsonStyle::compact, bool sorted_keys = false);

void append_json_string(std::string& out, std::string_view text);

}  // namespace blogin
