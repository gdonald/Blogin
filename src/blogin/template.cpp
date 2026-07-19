#include "template.h"

#include <string>
#include <vector>

#include "counters.h"
#include "html.h"

namespace blogin {
namespace {

struct Line {
  std::size_t indent = 0;
  std::string_view content;
};

bool is_blank(std::string_view line) {
  return line.find_first_not_of(" \t") == std::string_view::npos;
}

std::string_view trim_end(std::string_view text) {
  const auto last = text.find_last_not_of(" \t\r");

  return last == std::string_view::npos ? std::string_view{} : text.substr(0, last + 1);
}

std::vector<Line> significant_lines(std::string_view source) {
  std::vector<Line> lines;
  std::size_t start = 0;

  while (start <= source.size()) {
    const auto newline = source.find('\n', start);
    const std::string_view raw =
      newline == std::string_view::npos ? source.substr(start) : source.substr(start, newline - start);

    if (!is_blank(raw)) {
      const auto first = raw.find_first_not_of(' ');

      lines.push_back(Line{first, trim_end(raw.substr(first))});
    }

    if (newline == std::string_view::npos) {
      break;
    }

    start = newline + 1;
  }

  return lines;
}

bool is_tag_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '-' || character == '_';
}

void attach(TemplateNode* parent, TemplateNode* child) {
  if (parent->last_child == nullptr) {
    parent->first_child = child;
    parent->last_child = child;
    return;
  }

  parent->last_child->next_sibling = child;
  parent->last_child = child;
}

TemplateNode* make_output(Arena& arena, std::string_view expression, bool escaped) {
  auto* node = arena.create<TemplateNode>();

  node->op = escaped ? TemplateOp::output_escaped : TemplateOp::output_raw;
  node->text = expression;

  if (expression == "yield") {
    node->op = TemplateOp::yield_body;
  }

  return node;
}

// `%tag`, plain text, `= expression`, and `!= expression`. Nesting comes from
// indentation. This is the shape of the real engine, not its full syntax.
TemplateNode* build_node(Arena& arena, std::string_view content) {
  if (content.starts_with("!=")) {
    const auto expression = content.substr(2);
    const auto first = expression.find_first_not_of(' ');

    return make_output(arena, first == std::string_view::npos ? std::string_view{} : expression.substr(first), false);
  }

  if (content.starts_with('=')) {
    const auto expression = content.substr(1);
    const auto first = expression.find_first_not_of(' ');

    return make_output(arena, first == std::string_view::npos ? std::string_view{} : expression.substr(first), true);
  }

  if (content.starts_with('%')) {
    std::size_t end = 1;

    while (end < content.size() && is_tag_character(content[end])) {
      ++end;
    }

    auto* node = arena.create<TemplateNode>();
    node->op = TemplateOp::element;
    node->tag = content.substr(1, end - 1);

    std::string_view rest = content.substr(end);

    if (const auto first = rest.find_first_not_of(' '); first != std::string_view::npos) {
      rest = rest.substr(first);

      attach(node, build_node(arena, rest));
    }

    return node;
  }

  auto* node = arena.create<TemplateNode>();
  node->op = TemplateOp::literal;
  node->text = content;

  return node;
}

void render_node(std::string& out, const TemplateNode* node, const Context& context) {
  switch (node->op) {
    case TemplateOp::root:
      break;

    case TemplateOp::element:
      out += '<';
      out += node->tag;
      out += '>';
      break;

    case TemplateOp::literal:
      out += node->text;
      break;

    case TemplateOp::output_escaped:
      append_escaped(out, context.lookup(node->text));
      break;

    case TemplateOp::output_raw:
      out += context.lookup(node->text);
      break;

    case TemplateOp::yield_body:
      out += context.body();
      break;
  }

  for (const TemplateNode* child = node->first_child; child != nullptr; child = child->next_sibling) {
    render_node(out, child, context);
  }

  if (node->op == TemplateOp::element) {
    out += "</";
    out += node->tag;
    out += ">\n";
  }
}

}  // namespace

std::string_view Context::lookup(std::string_view name) const {
  const auto found = values_.find(std::string(name));

  return found == values_.end() ? std::string_view{} : std::string_view(found->second);
}

CompiledTemplate CompiledTemplate::compile(std::string source) {
  CompiledTemplate compiled;

  compiled.source_ = std::make_unique<Source>(std::move(source));
  compiled.arena_ = std::make_unique<Arena>();

  auto* root = compiled.arena_->create<TemplateNode>();

  const std::vector<Line> lines = significant_lines(compiled.source_->view());

  std::vector<TemplateNode*> open_nodes{root};
  std::vector<std::size_t> open_indents{0};

  for (const Line& line : lines) {
    while (open_nodes.size() > 1 && line.indent <= open_indents.back()) {
      open_nodes.pop_back();
      open_indents.pop_back();
    }

    TemplateNode* node = build_node(*compiled.arena_, line.content);

    attach(open_nodes.back(), node);

    open_nodes.push_back(node);
    open_indents.push_back(line.indent);
  }

  compiled.root_ = root;

  count(Counter::templates_compiled);

  return compiled;
}

void render_template(std::string& out, const CompiledTemplate& compiled, const Context& context) {
  render_node(out, compiled.root(), context);

  count(Counter::pages_rendered);
}

std::string render_template(const CompiledTemplate& compiled, const Context& context) {
  std::string out;
  out.reserve(4096);

  render_template(out, compiled, context);

  return out;
}

}  // namespace blogin
