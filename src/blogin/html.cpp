#include "html.h"

#include <format>

#include "highlight.h"
#include "slug.h"
#include "text.h"

namespace blogin {
namespace {

bool is_hex_digit(char character) {
  return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

// A block element starts on its own line, but only one newline separates it
// from what came before. This is what keeps list items and nested lists laid
// out the way the specification writes them.
void ensure_newline(std::string& out) {
  if (!out.empty() && out.back() != '\n') {
    out += '\n';
  }
}

// A tight list drops the paragraph wrappers inside its items.
bool in_tight_list(const Node* node) {
  const Node* item = node->parent;

  if (item == nullptr || item->kind != NodeKind::item) {
    return false;
  }

  const Node* list = item->parent;

  return list != nullptr && list->kind == NodeKind::list && list->tight;
}

// One walk produces the markup, the stripped text that summaries and the search
// index read, and the headings a table of contents is built from. Walking three
// times would cost three times as much and let the three drift apart.
class Renderer {
public:
  explicit Renderer(const RenderOptions& options) : options_(options) {}

  RenderResult run(const Node* node) {
    render(node);

    RenderResult result;
    result.html = wrapped(std::move(html_));
    result.text = std::string(text::trim(text_));
    result.headings = std::move(headings_);

    return result;
  }

private:
  // Some frameworks style the elements a document is made of only inside a
  // named wrapper. Bulma is the one that ships here: bare headings, lists,
  // blockquotes, and tables carry no styling until they sit inside `.content`.
  // A profile with no `article` class gets no wrapper and the same bytes as
  // before.
  std::string wrapped(std::string body) const {
    const std::string_view article = options_.framework.class_for("article");

    if (article.empty() || body.empty()) {
      return body;
    }

    std::string out;
    out.reserve(body.size() + article.size() + 24);
    out += "<div class=\"";
    append_escaped(out, article);
    out += "\">\n";
    out += body;

    if (out.back() != '\n') {
      out += '\n';
    }

    out += "</div>\n";

    return out;
  }

  void children(const Node* node) {
    for (const Node* child = node->first_child; child != nullptr; child = child->next_sibling) {
      render(child);
    }
  }

  void with_class(std::string_view slot) {
    const std::string_view classes = options_.framework.class_for(slot);

    if (classes.empty()) {
      return;
    }

    html_ += " class=\"";
    append_escaped(html_, classes);
    html_ += '"';
  }

  // An image's alt text is what its children say with the markup dropped.
  void collect_plain(std::string& out, const Node* node) const {
    for (const Node* child = node->first_child; child != nullptr; child = child->next_sibling) {
      switch (child->kind) {
        case NodeKind::text:
        case NodeKind::code_span:
        case NodeKind::math:
          out += child->literal;
          break;
        case NodeKind::soft_break:
        case NodeKind::line_break:
          out += '\n';
          break;
        default:
          collect_plain(out, child);
          break;
      }
    }
  }

  std::string plain_of(const Node* node) const {
    std::string out;
    collect_plain(out, node);

    return out;
  }

  // Classes from a {.one .two} block combine, the way a reader expects.
  void append_attributes(const Node* node, std::string_view extra) {
    std::string classes(extra);

    for (const Attribute* attribute = node->attributes; attribute != nullptr; attribute = attribute->next) {
      if (attribute->name != "class") {
        continue;
      }

      classes += classes.empty() ? std::string(attribute->value) : " " + std::string(attribute->value);
    }

    if (!classes.empty()) {
      html_ += " class=\"";
      append_escaped(html_, classes);
      html_ += '"';
    }

    for (const Attribute* attribute = node->attributes; attribute != nullptr; attribute = attribute->next) {
      if (attribute->name == "class") {
        continue;
      }

      html_ += ' ';
      html_ += attribute->name;
      html_ += "=\"";
      append_escaped(html_, attribute->value);
      html_ += '"';
    }
  }

  void render_code_block(const Node* node) {
    const std::string language(text::trim(node->info));
    const std::string lowered = text::to_lower_ascii(language);

    // Two info strings mean something other than a language.
    if (lowered == "mermaid") {
      ensure_newline(html_);
      html_ += "<pre class=\"mermaid\">";
      append_escaped(html_, node->literal);
      html_ += "</pre>\n";
      text_ += node->literal;

      return;
    }

    if (lowered == "math") {
      ensure_newline(html_);
      html_ += "<div class=\"math math-display\">";
      append_escaped(html_, text::trim(node->literal));
      html_ += "</div>\n";
      text_ += node->literal;

      return;
    }

    ensure_newline(html_);
    html_ += "<pre><code";

    std::string classes;

    if (!language.empty()) {
      classes = "language-" + language.substr(0, language.find_first_of(" \t"));
    }

    // A highlighted document says which blocks it could not highlight, so a
    // reader is not left wondering why one came out plain.
    if (options_.highlight && !language.empty() && !highlight::supports(language)) {
      classes += classes.empty() ? "hl-plain" : " hl-plain";
    }

    if (const std::string_view extra = options_.framework.class_for("code-block"); !extra.empty()) {
      classes += classes.empty() ? std::string(extra) : " " + std::string(extra);
    }

    if (!classes.empty()) {
      html_ += " class=\"";
      append_escaped(html_, classes);
      html_ += '"';
    }

    html_ += '>';

    if (options_.highlight && highlight::supports(language)) {
      html_ += highlight::render(node->literal, language);
    } else {
      append_escaped(html_, node->literal);
    }

    html_ += "</code></pre>\n";
    text_ += node->literal;
  }

  void render_heading(const Node* node) {
    const std::string title = plain_of(node);
    const std::string id = slug::slugify(title);

    headings_.push_back(Heading{node->level, title, id});

    ensure_newline(html_);
    html_ += std::format("<h{}", node->level);

    if (options_.heading_anchors && !id.empty()) {
      html_ += " id=\"";
      append_escaped(html_, id);
      html_ += '"';
    }

    with_class("heading");
    html_ += '>';

    children(node);

    if (options_.heading_anchors && !id.empty()) {
      html_ += R"(<a class="anchor" href="#)";
      append_escaped(html_, id);
      html_ += "\">#</a>";
    }

    html_ += std::format("</h{}>\n", node->level);
    text_ += '\n';
  }

  void render(const Node* node);
  void render_block(const Node* node);
  void render_table(const Node* node);
  void render_definition(const Node* node);
  void render_footnote(const Node* node);
  void render_inline(const Node* node);
  void render_link(const Node* node);

  const RenderOptions& options_;
  std::string html_;
  std::string text_;
  std::vector<Heading> headings_;
};

void Renderer::render(const Node* node) {
  switch (node->kind) {
    case NodeKind::document:
    case NodeKind::paragraph:
    case NodeKind::heading:
    case NodeKind::thematic_break:
    case NodeKind::block_quote:
    case NodeKind::list:
    case NodeKind::item:
    case NodeKind::code_block:
    case NodeKind::html_block:
      render_block(node);
      return;

    case NodeKind::table:
    case NodeKind::table_row:
    case NodeKind::table_cell:
      render_table(node);
      return;

    case NodeKind::definition_list:
    case NodeKind::definition_term:
    case NodeKind::definition_detail:
      render_definition(node);
      return;

    case NodeKind::footnotes:
    case NodeKind::footnote_item:
    case NodeKind::footnote_ref:
      render_footnote(node);
      return;

    case NodeKind::shortcode:
    case NodeKind::text:
    case NodeKind::soft_break:
    case NodeKind::line_break:
    case NodeKind::code_span:
    case NodeKind::html_inline:
    case NodeKind::emphasis:
    case NodeKind::strong:
    case NodeKind::strikethrough:
    case NodeKind::math:
      render_inline(node);
      return;

    case NodeKind::link:
    case NodeKind::image:
      render_link(node);
      return;
  }
}

void Renderer::render_table(const Node* node) {
  std::string& out = html_;

  switch (node->kind) {
    case NodeKind::table:
      ensure_newline(out);
      out += "<table";
      with_class("table");
      out += ">\n";
      children(node);
      out += "</table>\n";
      text_ += '\n';
      return;

    case NodeKind::table_row: {
      const bool head = node->header;

      ensure_newline(out);

      if (head) {
        out += "<thead>\n";
      }

      out += "<tr>\n";
      children(node);
      out += "</tr>\n";

      if (head) {
        out += "</thead>\n<tbody>\n";
      }

      if (!head && node->next_sibling == nullptr) {
        out += "</tbody>\n";
      }

      return;
    }

    case NodeKind::table_cell:
    default: {
      const char* tag = node->header ? "th" : "td";

      out += '<';
      out += tag;

      switch (node->alignment) {
        case CellAlignment::left: out += " align=\"left\""; break;
        case CellAlignment::center: out += " align=\"center\""; break;
        case CellAlignment::right: out += " align=\"right\""; break;
        case CellAlignment::none: break;
      }

      out += '>';
      children(node);
      out += "</";
      out += tag;
      out += ">\n";
      text_ += ' ';
      return;
    }
  }
}

void Renderer::render_definition(const Node* node) {
  std::string& out = html_;

  switch (node->kind) {
    case NodeKind::definition_list:
      ensure_newline(out);
      out += "<dl";
      with_class("definition-list");
      out += ">\n";
      children(node);
      out += "</dl>\n";
      return;

    case NodeKind::definition_term:
      ensure_newline(out);
      out += "<dt>";
      children(node);
      out += "</dt>\n";
      text_ += '\n';
      return;

    case NodeKind::definition_detail:
    default:
      ensure_newline(out);
      out += "<dd>";
      children(node);
      out += "</dd>\n";
      text_ += '\n';
      return;
  }
}

void Renderer::render_footnote(const Node* node) {
  std::string& out = html_;

  switch (node->kind) {
    case NodeKind::footnotes:
      ensure_newline(out);
      out += "<section class=\"footnotes\">\n<ol>\n";
      children(node);
      out += "</ol>\n</section>\n";
      return;

    case NodeKind::footnote_item:
      ensure_newline(out);
      out += "<li id=\"fn-";
      append_escaped(out, node->label);
      out += "\">";
      children(node);
      out += " <a href=\"#fnref-";
      append_escaped(out, node->label);
      out += "\" class=\"footnote-back\">&#8617;</a></li>\n";
      return;

    case NodeKind::footnote_ref:
    default:
      out += R"(<sup class="footnote-ref"><a href="#fn-)";
      append_escaped(out, node->label);
      out += "\" id=\"fnref-";
      append_escaped(out, node->label);

      if (node->occurrence > 1) {
        out += std::format("-{}", node->occurrence);
      }

      out += "\">";
      out += std::format("{}", node->number);
      out += "</a></sup>";
      return;
  }
}

void Renderer::render_link(const Node* node) {
  std::string& out = html_;

  switch (node->kind) {
    case NodeKind::link:
      out += "<a href=\"";
      append_url_escaped(out, node->url);
      out += '"';

      if (!node->title.empty()) {
        out += " title=\"";
        append_escaped(out, node->title);
        out += '"';
      }

      append_attributes(node, {});
      out += '>';
      children(node);
      out += "</a>";
      return;

    case NodeKind::image:
    default: {
      out += "<img src=\"";
      append_url_escaped(out, node->url);
      out += "\" alt=\"";

      const std::string alt = plain_of(node);
      append_escaped(out, alt);
      text_ += alt;

      out += '"';

      if (!node->title.empty()) {
        out += " title=\"";
        append_escaped(out, node->title);
        out += '"';
      }

      append_attributes(node, options_.framework.class_for("image"));
      out += " />";
      return;
    }
  }
}

void Renderer::render_block(const Node* node) {
  std::string& out = html_;

  switch (node->kind) {
    case NodeKind::document:
      children(node);
      return;

    case NodeKind::paragraph:
      if (in_tight_list(node)) {
        children(node);
        text_ += '\n';
        return;
      }

      ensure_newline(out);
      out += "<p>";
      children(node);
      out += "</p>\n";
      text_ += '\n';
      return;

    case NodeKind::heading:
      render_heading(node);
      return;

    case NodeKind::thematic_break:
      ensure_newline(out);
      out += "<hr />\n";
      return;

    case NodeKind::block_quote:
      ensure_newline(out);
      out += "<blockquote";
      with_class("blockquote");
      out += ">\n";
      children(node);
      ensure_newline(out);
      out += "</blockquote>\n";
      return;

    case NodeKind::list:
      ensure_newline(out);
      out += node->ordered ? "<ol" : "<ul";

      if (node->ordered && node->start != 1) {
        out += std::format(" start=\"{}\"", node->start);
      }

      with_class("list");
      out += ">\n";
      children(node);
      out += node->ordered ? "</ol>\n" : "</ul>\n";
      return;

    case NodeKind::item:
      ensure_newline(out);
      out += "<li>";

      if (node->task) {
        out += node->checked ? R"(<input type="checkbox" checked="" disabled="" /> )"
                             : R"(<input type="checkbox" disabled="" /> )";
      }

      children(node);
      out += "</li>\n";
      return;

    case NodeKind::code_block:
      render_code_block(node);
      return;

    case NodeKind::html_block:
    default:
      ensure_newline(out);
      out += node->literal;
      return;
  }
}

void Renderer::render_inline(const Node* node) {
  std::string& out = html_;

  switch (node->kind) {
    case NodeKind::shortcode:
      ensure_newline(out);

      if (options_.shortcodes != nullptr) {
        out += options_.shortcodes->expand(node->label, node->raw, node->literal);
      } else {
        out += "<p>";
        append_escaped(out, node->literal);
        out += "</p>";
      }

      out += '\n';
      return;

    case NodeKind::text:
      append_escaped(out, node->literal);
      text_ += node->literal;
      return;

    case NodeKind::soft_break:
      out += "\n";
      text_ += ' ';
      return;

    case NodeKind::line_break:
      out += "<br />\n";
      text_ += ' ';
      return;

    case NodeKind::code_span:
      out += "<code>";
      append_escaped(out, node->literal);
      out += "</code>";
      text_ += node->literal;
      return;

    case NodeKind::html_inline:
      out += node->literal;
      return;

    case NodeKind::emphasis:
      out += "<em>";
      children(node);
      out += "</em>";
      return;

    case NodeKind::strong:
      out += "<strong>";
      children(node);
      out += "</strong>";
      return;

    case NodeKind::strikethrough:
      out += "<del>";
      children(node);
      out += "</del>";
      return;

    case NodeKind::math:
    default:
      out += node->display ? "<span class=\"math math-display\">" : "<span class=\"math math-inline\">";
      append_escaped(out, node->literal);
      out += "</span>";
      text_ += node->literal;
      return;
  }
}

}  // namespace

void append_escaped(std::string& out, std::string_view text) {
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

void append_attribute_escaped(std::string& out, std::string_view text) {
  append_escaped(out, text);
}

void append_url_escaped(std::string& out, std::string_view url) {
  constexpr std::string_view safe = "-_.+!*'(),%#@?=;:/,+&$~";

  for (std::size_t index = 0; index < url.size(); ++index) {
    const char character = url[index];
    const auto byte = static_cast<unsigned char>(character);

    if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9')) {
      out += character;
      continue;
    }

    if (character == '&') {
      out += "&amp;";
      continue;
    }

    if (character == '%' && index + 2 < url.size() && is_hex_digit(url[index + 1]) &&
        is_hex_digit(url[index + 2])) {
      out += character;
      continue;
    }

    if (safe.contains(character)) {
      out += character;
      continue;
    }

    out += std::format("%{:02X}", byte);
  }
}

RenderResult render_document(const Node* node, const RenderOptions& options) {
  Renderer renderer(options);

  return renderer.run(node);
}

void render_html(std::string& out, const Node* node) {
  out += render_document(node).html;
}

std::string render_html(const Node* node) {
  return render_document(node).html;
}

}  // namespace blogin
