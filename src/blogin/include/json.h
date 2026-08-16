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

// The deepest nesting of objects and arrays parse_json accepts. Deep enough for
// any configuration or data file, shallow enough that building and unwinding the
// nested Value cannot overflow a worker thread's stack. Real documents nest
// fewer than ten levels.
//
// Anything written as JSON has to stay inside this to be read back, so the YAML
// parser checks what it built against it rather than carrying a second number
// that could drift.
inline constexpr std::size_t max_json_depth = 64;

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
