#include "json.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>

#include "text.h"

namespace blogin {
namespace {

class Parser {
public:
  explicit Parser(std::string_view text) : text_(text) {}

  std::expected<Value, ParseError> parse() {
    skip_whitespace();

    std::expected<Value, ParseError> result = parse_value();

    if (!result) {
      return result;
    }

    skip_whitespace();

    if (position_ < text_.size()) {
      return fail("unexpected trailing content");
    }

    return result;
  }

private:
  std::unexpected<ParseError> fail(std::string message) const {
    return std::unexpected(ParseError{std::move(message), line_, column_});
  }

  bool done() const { return position_ >= text_.size(); }

  char peek() const { return text_[position_]; }

  char advance() {
    const char character = text_[position_++];

    if (character == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }

    return character;
  }

  void skip_whitespace() {
    while (!done()) {
      const char character = peek();

      if (character == ' ' || character == '\t' || character == '\n' || character == '\r') {
        advance();
        continue;
      }

      break;
    }
  }

  bool consume(char expected) {
    if (done() || peek() != expected) {
      return false;
    }

    advance();

    return true;
  }

  bool consume_word(std::string_view word) {
    if (text_.compare(position_, word.size(), word) != 0) {
      return false;
    }

    for (std::size_t index = 0; index < word.size(); ++index) {
      advance();
    }

    return true;
  }

  std::expected<Value, ParseError> parse_value() {
    if (done()) {
      return fail("unexpected end of input");
    }

    // Only an object or an array nests, so only those cost a level. Charging a
    // scalar for one would make the limit depend on whether the innermost value
    // happened to be a scalar, and a data file nested to the limit YAML allows
    // would serialize to JSON this parser then refused to read back.
    const bool nests = peek() == '{' || peek() == '[';

    if (nests && ++depth_ > max_depth) {
      --depth_;

      return fail("nested too deeply");
    }

    std::expected<Value, ParseError> result = parse_value_inner();

    if (nests) {
      --depth_;
    }

    return result;
  }

  std::expected<Value, ParseError> parse_value_inner() {
    switch (peek()) {
      case '{':
        return parse_object();
      case '[':
        return parse_array();
      case '"':
        return parse_string_value();
      case 't':
        return consume_word("true") ? std::expected<Value, ParseError>(Value(true)) : fail("expected true");
      case 'f':
        return consume_word("false") ? std::expected<Value, ParseError>(Value(false)) : fail("expected false");
      case 'n':
        return consume_word("null") ? std::expected<Value, ParseError>(Value()) : fail("expected null");
      default:
        return parse_number();
    }
  }

  std::expected<Value, ParseError> parse_object() {
    advance();

    Value object = Value::object();

    skip_whitespace();

    if (consume('}')) {
      return object;
    }

    while (true) {
      skip_whitespace();

      if (done()) {
        return fail("unexpected end of input inside an object");
      }

      if (peek() != '"') {
        return fail("expected a key in double quotes");
      }

      std::expected<std::string, ParseError> key = parse_string();

      if (!key) {
        return std::unexpected(key.error());
      }

      skip_whitespace();

      if (!consume(':')) {
        return fail("expected ':' after a key");
      }

      skip_whitespace();

      std::expected<Value, ParseError> item = parse_value();

      if (!item) {
        return item;
      }

      object.set(std::move(*key), std::move(*item));

      skip_whitespace();

      if (consume(',')) {
        skip_whitespace();

        if (!done() && peek() == '}') {
          return fail("trailing comma before '}'");
        }

        continue;
      }

      if (consume('}')) {
        return object;
      }

      return fail("expected ',' or '}' in an object");
    }
  }

  std::expected<Value, ParseError> parse_array() {
    advance();

    Value array = Value::array();

    skip_whitespace();

    if (consume(']')) {
      return array;
    }

    while (true) {
      skip_whitespace();

      std::expected<Value, ParseError> item = parse_value();

      if (!item) {
        return item;
      }

      array.push(std::move(*item));

      skip_whitespace();

      if (consume(',')) {
        skip_whitespace();

        if (!done() && peek() == ']') {
          return fail("trailing comma before ']'");
        }

        continue;
      }

      if (consume(']')) {
        return array;
      }

      return fail("expected ',' or ']' in an array");
    }
  }

  static void append_utf8(std::string& out, std::uint32_t code_point) {
    if (code_point < 0x80U) {
      out += static_cast<char>(code_point);
    } else if (code_point < 0x800U) {
      out += static_cast<char>(0xC0U | (code_point >> 6));
      out += static_cast<char>(0x80U | (code_point & 0x3FU));
    } else if (code_point < 0x10000U) {
      out += static_cast<char>(0xE0U | (code_point >> 12));
      out += static_cast<char>(0x80U | ((code_point >> 6) & 0x3FU));
      out += static_cast<char>(0x80U | (code_point & 0x3FU));
    } else {
      out += static_cast<char>(0xF0U | (code_point >> 18));
      out += static_cast<char>(0x80U | ((code_point >> 12) & 0x3FU));
      out += static_cast<char>(0x80U | ((code_point >> 6) & 0x3FU));
      out += static_cast<char>(0x80U | (code_point & 0x3FU));
    }
  }

  std::expected<std::uint32_t, ParseError> parse_hex4() {
    if (position_ + 4 > text_.size()) {
      return std::unexpected(fail("incomplete \\u escape").error());
    }

    std::uint32_t value = 0;

    for (int index = 0; index < 4; ++index) {
      const char character = advance();
      value <<= 4;

      if (character >= '0' && character <= '9') {
        value |= static_cast<std::uint32_t>(character - '0');
      } else if (character >= 'a' && character <= 'f') {
        value |= static_cast<std::uint32_t>(character - 'a' + 10);
      } else if (character >= 'A' && character <= 'F') {
        value |= static_cast<std::uint32_t>(character - 'A' + 10);
      } else {
        return std::unexpected(fail("invalid hex digit in a \\u escape").error());
      }
    }

    return value;
  }

  std::expected<std::string, ParseError> parse_string() {
    advance();

    std::string out;

    while (true) {
      if (done()) {
        return std::unexpected(fail("unterminated string").error());
      }

      const char character = advance();

      if (character == '"') {
        return out;
      }

      if (character != '\\') {
        if (static_cast<unsigned char>(character) < 0x20U) {
          return std::unexpected(fail("unescaped control character in a string").error());
        }

        out += character;
        continue;
      }

      if (done()) {
        return std::unexpected(fail("unterminated escape").error());
      }

      switch (const char escape = advance(); escape) {
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        case '/':
          out += '/';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'u': {
          std::expected<std::uint32_t, ParseError> code_point = parse_hex4();

          if (!code_point) {
            return std::unexpected(code_point.error());
          }

          std::uint32_t value = *code_point;

          // A character outside the basic plane arrives as a surrogate pair.
          if (value >= 0xD800U && value <= 0xDBFFU && position_ + 1 < text_.size() && text_[position_] == '\\' &&
              text_[position_ + 1] == 'u') {
            advance();
            advance();

            std::expected<std::uint32_t, ParseError> low = parse_hex4();

            if (!low) {
              return std::unexpected(low.error());
            }

            if (*low >= 0xDC00U && *low <= 0xDFFFU) {
              value = 0x10000U + ((value - 0xD800U) << 10) + (*low - 0xDC00U);
            } else {
              append_utf8(out, value);
              value = *low;
            }
          }

          append_utf8(out, value);
          break;
        }
        default:
          return std::unexpected(fail("unknown escape").error());
      }
    }
  }

  std::expected<Value, ParseError> parse_string_value() {
    std::expected<std::string, ParseError> text = parse_string();

    if (!text) {
      return std::unexpected(text.error());
    }

    return Value(std::move(*text));
  }

  std::expected<Value, ParseError> parse_number() {
    const std::size_t start = position_;

    if (!done() && peek() == '-') {
      advance();
    }

    bool any_digits = false;

    while (!done() && peek() >= '0' && peek() <= '9') {
      advance();
      any_digits = true;
    }

    if (!any_digits) {
      return fail("expected a value");
    }

    bool floating = false;

    if (!done() && peek() == '.') {
      floating = true;
      advance();

      bool fraction_digits = false;

      while (!done() && peek() >= '0' && peek() <= '9') {
        advance();
        fraction_digits = true;
      }

      if (!fraction_digits) {
        return fail("expected a digit after '.'");
      }
    }

    if (!done() && (peek() == 'e' || peek() == 'E')) {
      floating = true;
      advance();

      if (!done() && (peek() == '+' || peek() == '-')) {
        advance();
      }

      bool exponent_digits = false;

      while (!done() && peek() >= '0' && peek() <= '9') {
        advance();
        exponent_digits = true;
      }

      if (!exponent_digits) {
        return fail("expected a digit in the exponent");
      }
    }

    const std::string_view token = text_.substr(start, position_ - start);

    if (!floating) {
      std::int64_t integer = 0;
      const auto result = std::from_chars(token.data(), token.data() + token.size(), integer);

      if (result.ec == std::errc{}) {
        return Value(integer);
      }
    }

    const std::optional<double> number = text::to_double(token);

    if (!number) {
      return fail("number out of range");
    }

    return Value(*number);
  }

  static constexpr std::size_t max_depth = max_json_depth;

  std::string_view text_;
  std::size_t position_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  std::size_t depth_ = 0;
};

void append_indent(std::string& out, int depth) {
  out.append(static_cast<std::size_t>(depth) * 2, ' ');
}

void write(std::string& out, const Value& value, JsonStyle style, bool sorted_keys, int depth) {
  const bool pretty = style == JsonStyle::pretty;

  switch (value.type()) {
    case Value::Type::null:
      out += "null";
      return;

    case Value::Type::boolean:
      out += value.as_boolean() ? "true" : "false";
      return;

    case Value::Type::integer:
      out += std::format("{}", value.as_integer());
      return;

    case Value::Type::number: {
      const double number = value.as_number();

      if (!std::isfinite(number)) {
        out += "null";
        return;
      }

      // A negative zero formats as "-0", which reads back as the integer 0 and
      // writes out as "0". JSON draws no distinction between the two zeroes, so
      // normalizing here makes writing a value twice produce the same
      // bytes twice. Builds that cache and compare their output depend on that.
      out += std::format("{}", number == 0.0 ? 0.0 : number);
      return;
    }

    case Value::Type::string:
      append_json_string(out, value.as_string());
      return;

    case Value::Type::array: {
      if (value.items().empty()) {
        out += "[]";
        return;
      }

      out += '[';

      bool first = true;

      for (const Value& item : value.items()) {
        if (!first) {
          out += ',';
        }

        first = false;

        if (pretty) {
          out += '\n';
          append_indent(out, depth + 1);
        }

        write(out, item, style, sorted_keys, depth + 1);
      }

      if (pretty) {
        out += '\n';
        append_indent(out, depth);
      }

      out += ']';
      return;
    }

    case Value::Type::object: {
      if (value.members().empty()) {
        out += "{}";
        return;
      }

      out += '{';

      Value::Object members = value.members();

      if (sorted_keys) {
        std::sort(members.begin(), members.end(),
                  [](const Value::Member& left, const Value::Member& right) { return left.first < right.first; });
      }

      bool first = true;

      for (const Value::Member& member : members) {
        if (!first) {
          out += ',';
        }

        first = false;

        if (pretty) {
          out += '\n';
          append_indent(out, depth + 1);
        }

        append_json_string(out, member.first);
        out += ':';

        if (pretty) {
          out += ' ';
        }

        write(out, member.second, style, sorted_keys, depth + 1);
      }

      if (pretty) {
        out += '\n';
        append_indent(out, depth);
      }

      out += '}';
      return;
    }
  }
}

}  // namespace

std::string ParseError::describe() const {
  return std::format("line {}, column {}: {}", line, column, message);
}

std::string ParseError::describe(std::string_view path) const {
  return std::format("{}:{}:{}: {}", path, line, column, message);
}

void append_json_string(std::string& out, std::string_view text) {
  out += '"';

  for (const char character : text) {
    switch (character) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(character) < 0x20U) {
          out += std::format("\\u{:04x}", static_cast<unsigned>(static_cast<unsigned char>(character)));
        } else {
          // Everything else, including UTF-8 sequences, passes through as-is.
          out += character;
        }
        break;
    }
  }

  out += '"';
}

std::expected<Value, ParseError> parse_json(std::string_view text) {
  Parser parser(text);

  return parser.parse();
}

std::string to_json(const Value& value, JsonStyle style, bool sorted_keys) {
  std::string out;
  out.reserve(256);

  write(out, value, style, sorted_keys, 0);

  if (style == JsonStyle::pretty) {
    out += '\n';
  }

  return out;
}

}  // namespace blogin
