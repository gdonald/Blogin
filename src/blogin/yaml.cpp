#include "yaml.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <utility>
#include <vector>

#include "json.h"
#include "text.h"

namespace blogin {
namespace {

struct Line {
  std::size_t indent = 0;
  std::string_view content;
  std::size_t number = 1;
};

// A comment runs from an unquoted # to the end of the line. Inside quotes a #
// is just a character, which is why this cannot be a plain find.
std::string_view strip_comment(std::string_view line) {
  char quote = '\0';

  for (std::size_t index = 0; index < line.size(); ++index) {
    const char character = line[index];

    if (quote != '\0') {
      if (character == quote) {
        quote = '\0';
      }

      continue;
    }

    if (character == '"' || character == '\'') {
      quote = character;
      continue;
    }

    if (character == '#' && (index == 0 || text::is_space(line[index - 1]))) {
      return line.substr(0, index);
    }
  }

  return line;
}

std::vector<Line> significant_lines(std::string_view source) {
  std::vector<Line> lines;
  std::size_t number = 0;

  for (const std::string_view raw : text::split_lines(source)) {
    ++number;

    const std::string_view without_comment = text::trim_end(strip_comment(raw));

    if (text::trim(without_comment).empty()) {
      continue;
    }

    if (text::trim(without_comment) == "---") {
      continue;
    }

    const std::size_t indent = without_comment.find_first_not_of(' ');

    lines.push_back(Line{indent, without_comment.substr(indent), number});
  }

  return lines;
}

bool all_digits(std::string_view text) {
  if (text.empty()) {
    return false;
  }

  return std::ranges::all_of(text, [](const char character) { return character >= '0' && character <= '9'; });
}

Value scalar_from(std::string_view token) {
  if (token.empty() || token == "~" || token == "null") {
    return Value();
  }

  if (token == "true") {
    return Value(true);
  }

  if (token == "false") {
    return Value(false);
  }

  std::string_view digits = token;
  const bool negative = digits.starts_with('-');

  if (negative) {
    digits.remove_prefix(1);
  }

  if (all_digits(digits)) {
    std::int64_t integer = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), integer);

    if (result.ec == std::errc{} && result.ptr == token.data() + token.size()) {
      return Value(integer);
    }
  }

  const auto dot = digits.find('.');

  if (dot != std::string_view::npos && all_digits(digits.substr(0, dot)) && all_digits(digits.substr(dot + 1))) {
    if (const std::optional<double> number = text::to_double(token)) {
      return Value(*number);
    }
  }

  return Value(std::string(token));
}

std::expected<Value, ParseError> unquote(std::string_view token, std::size_t line_number) {
  if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
    const std::string_view inner = token.substr(1, token.size() - 2);
    std::string out;

    for (std::size_t index = 0; index < inner.size(); ++index) {
      if (inner[index] != '\\' || index + 1 >= inner.size()) {
        out += inner[index];
        continue;
      }

      switch (const char escape = inner[++index]; escape) {
        case 'n':
          out += '\n';
          break;
        case 't':
          out += '\t';
          break;
        case 'r':
          out += '\r';
          break;
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        default:
          return std::unexpected(ParseError{"unknown escape in a double-quoted scalar", line_number, 1});
      }
    }

    return Value(std::move(out));
  }

  if (token.size() >= 2 && token.front() == '\'' && token.back() == '\'') {
    return Value(std::string(token.substr(1, token.size() - 2)));
  }

  return scalar_from(token);
}

std::expected<Value, ParseError> parse_flow_sequence(std::string_view token, std::size_t line_number) {
  Value list = Value::array();

  std::string_view inner = text::trim(token.substr(1, token.size() - 2));

  if (inner.empty()) {
    return list;
  }

  std::size_t start = 0;
  char quote = '\0';

  for (std::size_t index = 0; index <= inner.size(); ++index) {
    const bool at_end = index == inner.size();
    const char character = at_end ? ',' : inner[index];

    if (!at_end && quote != '\0') {
      if (character == quote) {
        quote = '\0';
      }

      continue;
    }

    if (!at_end && (character == '"' || character == '\'')) {
      quote = character;
      continue;
    }

    if (character != ',') {
      continue;
    }

    const std::string_view piece = text::trim(inner.substr(start, index - start));

    if (!piece.empty()) {
      std::expected<Value, ParseError> item = unquote(piece, line_number);

      if (!item) {
        return item;
      }

      list.push(std::move(*item));
    }

    start = index + 1;
  }

  return list;
}

std::expected<Value, ParseError> parse_scalar(std::string_view token, std::size_t line_number) {
  const std::string_view trimmed = text::trim(token);

  if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
    return parse_flow_sequence(trimmed, line_number);
  }

  if (trimmed.starts_with('{')) {
    return std::unexpected(ParseError{"flow mappings are not supported", line_number, 1});
  }

  if (trimmed.starts_with('&') || trimmed.starts_with('*')) {
    return std::unexpected(ParseError{"anchors and aliases are not supported", line_number, 1});
  }

  if (trimmed == "|" || trimmed == ">" || trimmed.starts_with("|-") || trimmed.starts_with(">-")) {
    return std::unexpected(ParseError{"block scalars are not supported", line_number, 1});
  }

  return unquote(trimmed, line_number);
}

// Splits "key: value" at the first colon that is not inside quotes.
std::optional<std::size_t> mapping_colon(std::string_view content) {
  char quote = '\0';

  for (std::size_t index = 0; index < content.size(); ++index) {
    const char character = content[index];

    if (quote != '\0') {
      if (character == quote) {
        quote = '\0';
      }

      continue;
    }

    if (character == '"' || character == '\'') {
      quote = character;
      continue;
    }

    if (character == ':' && (index + 1 == content.size() || text::is_space(content[index + 1]))) {
      return index;
    }
  }

  return std::nullopt;
}

class Parser {
public:
  explicit Parser(std::vector<Line> lines) : lines_(std::move(lines)) {}

  std::expected<Value, ParseError> parse() {
    if (lines_.empty()) {
      return Value::object();
    }

    return parse_block(lines_[0].indent);
  }

private:
  // Every nesting level in a data file comes back through here, so counting the
  // trips down bounds the recursion. Without it, a file indented tens of
  // thousands of levels deep exhausts the stack, which is a crash rather than
  // the line number this parser promises. JSON has had the same guard.
  std::expected<Value, ParseError> parse_block(std::size_t indent) {
    if (++depth_ > max_depth) {
      --depth_;

      const std::size_t line_number =
        position_ < lines_.size() ? lines_[position_].number : lines_.size();

      return std::unexpected(ParseError{"nested too deeply", line_number, 1});
    }

    std::expected<Value, ParseError> result = parse_block_inner(indent);

    --depth_;

    return result;
  }

  std::expected<Value, ParseError> parse_block_inner(std::size_t indent) {
    if (position_ >= lines_.size()) {
      return Value();
    }

    if (lines_[position_].content.starts_with("- ") || lines_[position_].content == "-") {
      return parse_sequence(indent);
    }

    return parse_mapping(indent);
  }

  std::expected<Value, ParseError> parse_mapping(std::size_t indent) {
    Value mapping = Value::object();

    while (position_ < lines_.size()) {
      const Line& line = lines_[position_];

      if (line.indent < indent) {
        break;
      }

      if (line.indent > indent) {
        return std::unexpected(ParseError{"unexpected indentation", line.number, line.indent + 1});
      }

      if (line.content.starts_with("- ")) {
        return std::unexpected(ParseError{"a sequence item where a mapping key was expected", line.number, 1});
      }

      const std::optional<std::size_t> colon = mapping_colon(line.content);

      if (!colon) {
        return std::unexpected(ParseError{"expected 'key: value'", line.number, 1});
      }

      std::expected<Value, ParseError> key = unquote(text::trim(line.content.substr(0, *colon)), line.number);

      if (!key) {
        return key;
      }

      const std::string_view rest = text::trim(line.content.substr(*colon + 1));
      const std::size_t current_indent = line.indent;

      ++position_;

      if (!rest.empty()) {
        std::expected<Value, ParseError> item = parse_scalar(rest, line.number);

        if (!item) {
          return item;
        }

        mapping.set(std::string(key->as_string(std::string_view(""))), std::move(*item));
        continue;
      }

      if (position_ < lines_.size() && lines_[position_].indent > current_indent) {
        std::expected<Value, ParseError> nested = parse_block(lines_[position_].indent);

        if (!nested) {
          return nested;
        }

        mapping.set(std::string(key->as_string(std::string_view(""))), std::move(*nested));
        continue;
      }

      // A sequence may sit at the same indentation as the key it belongs to.
      if (position_ < lines_.size() && lines_[position_].indent == current_indent &&
          lines_[position_].content.starts_with("- ")) {
        std::expected<Value, ParseError> nested = parse_sequence(current_indent);

        if (!nested) {
          return nested;
        }

        mapping.set(std::string(key->as_string(std::string_view(""))), std::move(*nested));
        continue;
      }

      mapping.set(std::string(key->as_string(std::string_view(""))), Value());
    }

    return mapping;
  }

  std::expected<Value, ParseError> parse_sequence(std::size_t indent) {
    Value sequence = Value::array();

    while (position_ < lines_.size()) {
      const Line& line = lines_[position_];

      if (line.indent < indent) {
        break;
      }

      if (line.indent > indent) {
        return std::unexpected(ParseError{"unexpected indentation", line.number, line.indent + 1});
      }

      if (!line.content.starts_with("- ") && line.content != "-") {
        break;
      }

      const std::string_view rest = text::trim(line.content.substr(1));
      const std::size_t current_indent = line.indent;

      ++position_;

      if (rest.empty()) {
        if (position_ < lines_.size() && lines_[position_].indent > current_indent) {
          std::expected<Value, ParseError> nested = parse_block(lines_[position_].indent);

          if (!nested) {
            return nested;
          }

          sequence.push(std::move(*nested));
          continue;
        }

        sequence.push(Value());
        continue;
      }

      // "- - a" packs a nested sequence onto the dash line. Guessing at it
      // would silently yield the string "- a", so it is refused instead.
      if (rest.starts_with("- ")) {
        return std::unexpected(
          ParseError{"a nested sequence on the same line is not supported. Indent it on its own line",
                     line.number, current_indent + 1});
      }

      // "- key: value" starts a mapping whose first key sits on the dash line.
      if (mapping_colon(rest).has_value()) {
        std::expected<Value, ParseError> item = parse_inline_mapping(rest, current_indent, line.number);

        if (!item) {
          return item;
        }

        sequence.push(std::move(*item));
        continue;
      }

      std::expected<Value, ParseError> item = parse_scalar(rest, line.number);

      if (!item) {
        return item;
      }

      sequence.push(std::move(*item));
    }

    return sequence;
  }

  std::expected<Value, ParseError> parse_inline_mapping(std::string_view first, std::size_t dash_indent,
                                                        std::size_t line_number) {
    Value mapping = Value::object();

    const std::optional<std::size_t> colon = mapping_colon(first);

    if (!colon) {
      return std::unexpected(ParseError{"expected 'key: value'", line_number, 1});
    }

    std::expected<Value, ParseError> key = unquote(text::trim(first.substr(0, *colon)), line_number);

    if (!key) {
      return key;
    }

    const std::string_view rest = text::trim(first.substr(*colon + 1));

    if (!rest.empty()) {
      std::expected<Value, ParseError> item = parse_scalar(rest, line_number);

      if (!item) {
        return item;
      }

      mapping.set(std::string(key->as_string(std::string_view(""))), std::move(*item));
    } else if (position_ < lines_.size() && lines_[position_].indent > dash_indent) {
      std::expected<Value, ParseError> nested = parse_block(lines_[position_].indent);

      if (!nested) {
        return nested;
      }

      mapping.set(std::string(key->as_string(std::string_view(""))), std::move(*nested));
    } else {
      mapping.set(std::string(key->as_string(std::string_view(""))), Value());
    }

    // Remaining keys of this item are indented past the dash.
    while (position_ < lines_.size() && lines_[position_].indent > dash_indent &&
           !lines_[position_].content.starts_with("- ")) {
      const std::size_t continuation_indent = lines_[position_].indent;

      std::expected<Value, ParseError> more = parse_mapping(continuation_indent);

      if (!more) {
        return more;
      }

      for (const Value::Member& member : more->members()) {
        mapping.set(member.first, member.second);
      }
    }

    return mapping;
  }

  std::vector<Line> lines_;
int depth_ = 0;

  // Deep enough for any data file, shallow enough that building and unwinding
  // the nested Value cannot overflow a worker thread's stack.
  //
  // Deep enough for any data file, shallow enough that building and unwinding
  // the nested Value cannot overflow a worker thread's stack. Real data files
  // nest fewer than ten levels.
  //
  // This bounds the recursion, not the value it builds. One block here can
  // produce more than one container, so what came back is measured separately
  // once it exists.
  static constexpr int max_depth = 64;

  std::size_t position_ = 0;
};

// How deeply the containers in a parsed value nest, walked with an explicit
// stack so that measuring a deep value cannot itself overflow.
//
// This is what the JSON parser counts, and it is not what the block counter
// above counts: `- k:` is one block here and an array holding an object there,
// so a file can stay inside the recursion limit and still describe a value
// nested past what JSON reads back. The shapes that do this are irregular
// enough that no fixed ratio between the two limits holds, so the value is
// measured rather than predicted.
std::size_t container_depth(const Value& root) {
  std::vector<std::pair<const Value*, std::size_t>> pending{{&root, 1}};
  std::size_t deepest = 0;

  while (!pending.empty()) {
    const auto [value, depth] = pending.back();

    pending.pop_back();

    if (!value->is_object() && !value->is_array()) {
      continue;
    }

    deepest = std::max(deepest, depth);

    if (value->is_object()) {
      for (const auto& [name, child] : value->members()) {
        pending.emplace_back(&child, depth + 1);
      }

      continue;
    }

    for (std::size_t index = 0; index < value->size(); ++index) {
      pending.emplace_back(&value->at(index), depth + 1);
    }
  }

  return deepest;
}

}  // namespace

std::expected<Value, ParseError> parse_yaml(std::string_view text) {
  Parser parser(significant_lines(text));

  auto parsed = parser.parse();

  if (!parsed) {
    return parsed;
  }

  // Data files are written as JSON on the way to the search index, so a value
  // this parser accepts has to be one that parser reads back. Refusing here is
  // an error naming the file. Letting it through is a build that writes an
  // index nothing can load.
  if (container_depth(*parsed) > max_json_depth) {
    return std::unexpected(ParseError{"nested too deeply", significant_lines(text).size(), 1});
  }

  return parsed;
}

}  // namespace blogin
