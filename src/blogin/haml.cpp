#include "haml.h"

#include <algorithm>
#include <format>
#include <utility>

#include "text.h"

namespace blogin::haml {
namespace {

bool is_name_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '-' || character == '_';
}

struct Line {
  std::size_t indent = 0;
  std::string_view content;
  std::size_t number = 1;
};

// A blank line separates nothing in HAML, so only lines with content are kept,
// each carrying the column its content starts at.
std::vector<Line> significant_lines(std::string_view source) {
  std::vector<Line> lines;
  std::size_t number = 0;

  for (const std::string_view raw : text::split_lines(source)) {
    ++number;

    if (text::trim(raw).empty()) {
      continue;
    }

    const std::size_t indent = raw.find_first_not_of(" \t");

    lines.push_back(Line{indent, text::trim_end(raw.substr(indent)), number});
  }

  return lines;
}

class Parser {
public:
  Parser(std::string_view source, std::string_view name) : lines_(significant_lines(source)), name_(name) {}

  std::expected<std::unique_ptr<Node>, ParseError> parse() {
    auto root = std::make_unique<Node>();

    while (position_ < lines_.size()) {
      auto child = parse_node(lines_[position_].indent);

      if (!child) {
        return std::unexpected(child.error());
      }

      root->children.push_back(std::move(*child));
    }

    return root;
  }

private:
  static std::unexpected<ParseError> fail(const Line& line, std::string message, std::size_t column = 1) {
    return std::unexpected(ParseError{std::move(message), line.number, column});
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_node(std::size_t indent) {
    const Line& line = lines_[position_++];

    auto node = build(line);

    if (!node) {
      return node;
    }

    // Everything indented past this line belongs to it.
    while (position_ < lines_.size() && lines_[position_].indent > indent) {
      const std::size_t child_indent = lines_[position_].indent;

      // A filter takes its block as raw text, not as HAML.
      if ((*node)->kind == NodeKind::filter) {
        auto child = std::make_unique<Node>();
        child->kind = NodeKind::text;
        child->line = lines_[position_].number;
        child->text.push_back(Segment{std::string(lines_[position_].content), nullptr});

        (*node)->children.push_back(std::move(child));
        ++position_;

        continue;
      }

      auto child = parse_node(child_indent);

      if (!child) {
        return child;
      }

      (*node)->children.push_back(std::move(*child));
    }

    return node;
  }

  std::expected<std::unique_ptr<Node>, ParseError> build(const Line& line) {
    const std::string_view content = line.content;

    if (content.starts_with("!!!")) {
      auto node = std::make_unique<Node>();
      node->kind = NodeKind::doctype;
      node->line = line.number;
      node->tag = std::string(text::trim(content.substr(3)));

      return node;
    }

    if (content.starts_with("-#") || content.starts_with("/")) {
      auto node = std::make_unique<Node>();
      node->kind = NodeKind::comment;
      node->line = line.number;
      node->text.push_back(Segment{std::string(text::trim(content.substr(content[0] == '/' ? 1 : 2))), nullptr});

      return node;
    }

    if (content.starts_with(':') && content.size() > 1 && is_name_character(content[1])) {
      auto node = std::make_unique<Node>();
      node->kind = NodeKind::filter;
      node->line = line.number;
      node->filter = std::string(text::trim(content.substr(1)));

      return node;
    }

    if (content.starts_with('-')) {
      return build_control(line, text::trim(content.substr(1)));
    }

    if (content.starts_with("!=") || content.starts_with("=")) {
      const bool escaped = content[0] == '=';
      const std::string_view source = text::trim(content.substr(escaped ? 1 : 2));

      return build_output(line, source, escaped);
    }

    if (content.starts_with('%') || content.starts_with('.') || content.starts_with('#')) {
      return build_element(line, content);
    }

    return build_text(line, content.starts_with('\\') ? content.substr(1) : content);
  }

  // Plain text, and what a tag's own line says after the tag. A leading
  // backslash is what a template writes to say that a line beginning with a
  // character HAML would otherwise read is text: `\= 2 + 2` is that text.
  static std::expected<std::unique_ptr<Node>, ParseError> build_text(const Line& line,
                                                                     std::string_view content) {
    auto node = std::make_unique<Node>();
    node->kind = NodeKind::text;
    node->line = line.number;

    auto parsed = parse_interpolated(line, content);

    if (!parsed) {
      return std::unexpected(parsed.error());
    }

    node->text = std::move(*parsed);

    return node;
  }

  static std::expected<std::unique_ptr<Node>, ParseError> build_output(const Line& line, std::string_view source,
                                                                       bool escaped) {
    auto node = std::make_unique<Node>();
    node->kind = NodeKind::output;
    node->line = line.number;
    node->escaped = escaped;

    auto parsed = expression::parse(source, line.number, line.indent + 1);

    if (!parsed) {
      return std::unexpected(parsed.error());
    }

    node->value = std::move(*parsed);

    return node;
  }

  static std::expected<std::unique_ptr<Node>, ParseError> build_control(const Line& line, std::string_view source) {
    auto node = std::make_unique<Node>();
    node->kind = NodeKind::control;
    node->line = line.number;

    const auto keyword_end = source.find_first_of(" \t");
    const std::string_view keyword = source.substr(0, keyword_end);
    const std::string_view rest =
      keyword_end == std::string_view::npos ? std::string_view{} : text::trim(source.substr(keyword_end));

    if (keyword == "if") {
      node->control = ControlKind::if_;
    } else if (keyword == "elsif") {
      node->control = ControlKind::elsif_;
    } else if (keyword == "else") {
      node->control = ControlKind::else_;

      return node;
    } else if (keyword == "unless") {
      node->control = ControlKind::unless_;
    } else if (keyword == "for") {
      node->control = ControlKind::for_;
    } else {
      return fail(line, std::format("'{}' is not something a template can do", keyword));
    }

    if (node->control == ControlKind::for_) {
      // "for items -> $name", the shape the layouts use.
      const auto arrow = rest.find("->");

      if (arrow == std::string_view::npos) {
        return fail(line, "a for loop needs '-> $name' to say what each item is called");
      }

      std::string_view variable = text::trim(rest.substr(arrow + 2));

      if (variable.starts_with('$')) {
        variable.remove_prefix(1);
      }

      if (variable.empty()) {
        return fail(line, "a for loop needs a name after '->'");
      }

      node->loop_variable = std::string(variable);

      auto parsed = expression::parse(text::trim(rest.substr(0, arrow)), line.number, line.indent + 1);

      if (!parsed) {
        return std::unexpected(parsed.error());
      }

      node->value = std::move(*parsed);

      return node;
    }

    auto parsed = expression::parse(rest, line.number, line.indent + 1);

    if (!parsed) {
      return std::unexpected(parsed.error());
    }

    node->value = std::move(*parsed);

    return node;
  }

  std::expected<std::unique_ptr<Node>, ParseError> build_element(const Line& line, std::string_view content) {
    auto node = std::make_unique<Node>();
    node->kind = NodeKind::element;
    node->line = line.number;
    node->tag = "div";

    std::size_t index = 0;

    if (content[index] == '%') {
      ++index;

      const std::size_t start = index;

      while (index < content.size() && (is_name_character(content[index]) || content[index] == ':')) {
        ++index;
      }

      node->tag = std::string(content.substr(start, index - start));
    }

    // .class and #id may repeat and may follow the tag in any order.
    while (index < content.size() && (content[index] == '.' || content[index] == '#')) {
      const char kind = content[index++];
      const std::size_t start = index;

      while (index < content.size() && is_name_character(content[index])) {
        ++index;
      }

      if (index == start) {
        return fail(line, std::format("expected a name after '{}'", kind), index + 1);
      }

      if (kind == '.') {
        node->classes.emplace_back(content.substr(start, index - start));
      } else {
        node->id = std::string(content.substr(start, index - start));
      }
    }

    if (index < content.size() && (content[index] == '{' || content[index] == '(')) {
      auto consumed = parse_attributes(line, content, index, *node);

      if (!consumed) {
        return std::unexpected(consumed.error());
      }

      index = *consumed;
    }

    if (index < content.size() && content[index] == '/') {
      node->self_closing = true;
      ++index;
    }

    const std::string_view rest = text::trim(content.substr(std::min(index, content.size())));

    if (rest.empty()) {
      return node;
    }

    // Anything after the tag on the same line is its only child, and it is
    // either an output expression or plain text. A '/' or a '-' there is a
    // character in that text, not the start of a comment or a control
    // line, which only a line of its own can be.
    Line inline_line = line;
    inline_line.content = rest;

    auto child = rest.starts_with("!=") || rest.starts_with('=')
                   ? build(inline_line)
                   : build_text(inline_line, rest.starts_with('\\') ? rest.substr(1) : rest);

    if (!child) {
      return child;
    }

    node->children.push_back(std::move(*child));

    return node;
  }

  // {key: 'value', 'other' => expr} and (key='value'), both of which appear in
  // the layouts.
  std::expected<std::size_t, ParseError> parse_attributes(const Line& line, std::string_view content,
                                                          std::size_t index, Node& node) {
    const char opener = content[index];
    const char closer = opener == '{' ? '}' : ')';

    int depth = 0;
    std::size_t scan = index;
    char quote = '\0';

    for (; scan < content.size(); ++scan) {
      const char character = content[scan];

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

      if (character == opener) {
        ++depth;
      } else if (character == closer) {
        if (--depth == 0) {
          break;
        }
      }
    }

    if (scan >= content.size()) {
      return std::unexpected(fail(line, "unterminated attribute list", index + 1).error());
    }

    const std::string_view body = content.substr(index + 1, scan - index - 1);

    if (auto parsed = parse_attribute_body(line, body, {}, node); !parsed) {
      return std::unexpected(parsed.error());
    }

    return scan + 1;
  }

  // `{data: {year: true}}` writes data-year, the way the layouts spell a
  // group of related attributes.
  std::expected<void, ParseError> parse_attribute_body(const Line& line, std::string_view body,
                                                       std::string_view prefix, Node& node) {
    std::size_t position = 0;

    while (position < body.size()) {
      while (position < body.size() && (body[position] == ' ' || body[position] == ',' ||
                                        body[position] == '\t')) {
        ++position;
      }

      if (position >= body.size()) {
        break;
      }

      std::string name;

      if (body[position] == '"' || body[position] == '\'') {
        const char name_quote = body[position];
        const std::size_t start = ++position;

        while (position < body.size() && body[position] != name_quote) {
          ++position;
        }

        name = std::string(body.substr(start, position - start));

        if (position < body.size()) {
          ++position;
        }
      } else {
        const std::size_t start = position;

        // A colon separates the name from its value, so it cannot be part of
        // the name itself. A namespaced name is written quoted.
        while (position < body.size() && is_name_character(body[position])) {
          ++position;
        }

        name = std::string(body.substr(start, position - start));
      }

      if (name.empty()) {
        return std::unexpected(fail(line, "expected an attribute name").error());
      }

      // "key: value", "key => value", and "key=value" all appear.
      while (position < body.size() && (body[position] == ' ' || body[position] == '\t')) {
        ++position;
      }

      if (body.compare(position, 2, "=>") == 0) {
        position += 2;
      } else if (position < body.size() && (body[position] == ':' || body[position] == '=')) {
        ++position;
      } else {
        // A name on its own is a bare attribute.
        Attribute attribute;
        attribute.name = prefix.empty() ? name : std::string(prefix) + "-" + name;
        attribute.boolean_shorthand = true;

        node.attributes.push_back(std::move(attribute));
        continue;
      }

      while (position < body.size() && (body[position] == ' ' || body[position] == '\t')) {
        ++position;
      }

      const std::size_t value_start = position;
      char value_quote = '\0';
      int value_depth = 0;

      while (position < body.size()) {
        const char character = body[position];

        if (value_quote != '\0') {
          if (character == value_quote) {
            value_quote = '\0';
          }

          ++position;
          continue;
        }

        if (character == '"' || character == '\'') {
          value_quote = character;
          ++position;
          continue;
        }

        if (character == '(' || character == '{' || character == '[') {
          ++value_depth;
        } else if (character == ')' || character == '}' || character == ']') {
          --value_depth;
        } else if (character == ',' && value_depth == 0) {
          break;
        }

        ++position;
      }

      const std::string_view raw = text::trim(body.substr(value_start, position - value_start));

      const std::string full_name = prefix.empty() ? name : std::string(prefix) + "-" + name;

      // A nested hash expands into one attribute per key, joined by a hyphen.
      if (raw.size() >= 2 && raw.front() == '{' && raw.back() == '}') {
        if (auto nested = parse_attribute_body(line, raw.substr(1, raw.size() - 2), full_name, node);
            !nested) {
          return nested;
        }

        continue;
      }

      Attribute attribute;
      attribute.name = full_name;

      // A quoted value may carry #{...}, so it is treated as text, not
      // as an expression.
      if (raw.size() >= 2 && (raw.front() == '"' || raw.front() == '\'') && raw.back() == raw.front()) {
        auto parsed = parse_interpolated(line, raw.substr(1, raw.size() - 2));

        if (!parsed) {
          return std::unexpected(parsed.error());
        }

        attribute.value = std::move(*parsed);
      } else {
        auto parsed = expression::parse(raw, line.number, line.indent + 1);

        if (!parsed) {
          return std::unexpected(parsed.error());
        }

        Segment segment;
        segment.hole = std::move(*parsed);
        attribute.value.push_back(std::move(segment));
      }

      node.attributes.push_back(std::move(attribute));
    }

    return {};
  }

  static std::expected<Interpolated, ParseError> parse_interpolated(const Line& line, std::string_view content) {
    Interpolated parts;
    std::string literal;

    std::size_t index = 0;

    while (index < content.size()) {
      if (content.compare(index, 2, "#{") != 0) {
        literal += content[index];
        ++index;
        continue;
      }

      int depth = 1;
      std::size_t scan = index + 2;

      while (scan < content.size() && depth > 0) {
        if (content[scan] == '{') {
          ++depth;
        } else if (content[scan] == '}') {
          --depth;
        }

        if (depth > 0) {
          ++scan;
        }
      }

      if (depth != 0) {
        return std::unexpected(fail(line, "unterminated '#{'", index + 1).error());
      }

      if (!literal.empty()) {
        parts.push_back(Segment{std::exchange(literal, {}), nullptr});
      }

      auto parsed = expression::parse(content.substr(index + 2, scan - index - 2), line.number, index + 1);

      if (!parsed) {
        return std::unexpected(parsed.error());
      }

      Segment segment;
      segment.hole = std::move(*parsed);
      parts.push_back(std::move(segment));

      index = scan + 1;
    }

    if (!literal.empty()) {
      parts.push_back(Segment{std::move(literal), nullptr});
    }

    return parts;
  }

  std::vector<Line> lines_;
  std::string_view name_;
  std::size_t position_ = 0;
};

}  // namespace

std::expected<Template, ParseError> Template::compile(std::string source, std::string name) {
  Template compiled;
  compiled.source_ = std::move(source);
  compiled.name_ = std::move(name);

  Parser parser(compiled.source_, compiled.name_);

  auto root = parser.parse();

  if (!root) {
    return std::unexpected(root.error());
  }

  compiled.root_ = std::move(*root);

  return compiled;
}

}  // namespace blogin::haml
