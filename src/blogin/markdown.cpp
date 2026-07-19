#include "markdown.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "markdown_internal.h"
#include "text.h"

namespace blogin {
namespace {

using namespace markdown_detail;

constexpr int code_indent = 4;
constexpr int max_nesting = 64;

// A tab advances to the next multiple of four, so indentation is counted in
// columns rather than in bytes.
//
// A tab can also straddle the point where indentation stops. When four columns
// of indentation end halfway through a tab, the columns it still owes belong to
// the content, which is why `>\t\tfoo` indents its code block by two spaces.
// Those owed columns are carried as `pending_spaces` and appear at the front of
// `rest()`.
struct Scanner {
  Scanner() = default;

  explicit Scanner(std::string_view text) : line(text) {}

  std::string_view line;
  std::size_t offset = 0;
  int column = 0;
  int pending_spaces = 0;

  bool done() const { return offset >= line.size() && pending_spaces == 0; }

  char peek() const {
    if (pending_spaces > 0) {
      return ' ';
    }

    return offset >= line.size() ? '\0' : line[offset];
  }

  void advance() {
    if (pending_spaces > 0) {
      --pending_spaces;
      ++column;

      return;
    }

    if (offset >= line.size()) {
      return;
    }

    column += line[offset] == '\t' ? 4 - (column % 4) : 1;
    ++offset;
  }

  int skip_spaces(int limit) {
    const int started = column;

    while (is_space_or_tab(peek()) && column - started < limit) {
      if (peek() == '\t' && pending_spaces == 0) {
        const int width = 4 - (column % 4);
        const int remaining = limit - (column - started);

        if (width > remaining) {
          // The tab crosses the stopping point. What is left of it becomes
          // content rather than indentation.
          pending_spaces = width - remaining;
          column += remaining;
          ++offset;

          return column - started;
        }
      }

      advance();
    }

    return column - started;
  }

  std::string_view rest() const {
    if (pending_spaces == 0) {
      return offset >= line.size() ? std::string_view{} : line.substr(offset);
    }

    expanded.assign(static_cast<std::size_t>(pending_spaces), ' ');

    if (offset < line.size()) {
      expanded += line.substr(offset);
    }

    return expanded;
  }

private:
  // Only used when a tab was split, which is rare, so the common path stays a
  // view straight into the line.
  mutable std::string expanded;
};

bool matches_thematic_break(std::string_view rest) {
  if (rest.empty()) {
    return false;
  }

  const char marker = rest[0];

  if (marker != '*' && marker != '-' && marker != '_') {
    return false;
  }

  int count = 0;

  for (const char character : rest) {
    if (character == marker) {
      ++count;
    } else if (!is_space_or_tab(character)) {
      return false;
    }
  }

  return count >= 3;
}

struct AtxMatch {
  bool matched = false;
  int level = 0;
  std::string_view content;
};

AtxMatch match_atx_heading(std::string_view rest) {
  AtxMatch match;

  std::size_t level = 0;

  while (level < rest.size() && rest[level] == '#') {
    ++level;
  }

  if (level == 0 || level > 6) {
    return match;
  }

  const std::string_view after = rest.substr(level);

  if (!after.empty() && !is_space_or_tab(after[0])) {
    return match;
  }

  std::string_view content = text::trim(after);
  std::size_t trailing = content.size();

  while (trailing > 0 && content[trailing - 1] == '#') {
    --trailing;
  }

  if (trailing == 0) {
    content = {};
  } else if (trailing < content.size() && is_space_or_tab(content[trailing - 1])) {
    content = text::trim_end(content.substr(0, trailing));
  }

  match.matched = true;
  match.level = static_cast<int>(level);
  match.content = content;

  return match;
}

struct FenceMatch {
  bool matched = false;
  char character = '\0';
  int length = 0;
  std::string_view info;
};

FenceMatch match_code_fence(std::string_view rest) {
  FenceMatch match;

  if (rest.empty() || (rest[0] != '`' && rest[0] != '~')) {
    return match;
  }

  const char marker = rest[0];
  std::size_t length = 0;

  while (length < rest.size() && rest[length] == marker) {
    ++length;
  }

  if (length < 3) {
    return match;
  }

  const std::string_view info = text::trim(rest.substr(length));

  // A backtick fence cannot carry a backtick in its info string, or a code span
  // running over a line break would read as a fence.
  if (marker == '`' && info.contains('`')) {
    return match;
  }

  match.matched = true;
  match.character = marker;
  match.length = static_cast<int>(length);
  match.info = info;

  return match;
}

bool matches_closing_fence(std::string_view rest, char marker, int length) {
  std::size_t count = 0;

  while (count < rest.size() && rest[count] == marker) {
    ++count;
  }

  return std::cmp_greater_equal(count, length) && text::trim(rest.substr(count)).empty();
}

struct ListMatch {
  bool matched = false;
  bool ordered = false;
  int start = 1;
  ListDelimiter delimiter = ListDelimiter::none;
  std::size_t marker_width = 0;
  char bullet = '\0';
};

ListMatch match_list_marker(std::string_view rest) {
  ListMatch match;

  if (rest.empty()) {
    return match;
  }

  if (rest[0] == '-' || rest[0] == '+' || rest[0] == '*') {
    match.matched = true;
    match.marker_width = 1;
    match.bullet = rest[0];

    return match;
  }

  std::size_t digits = 0;

  while (digits < rest.size() && digits < 10 && is_digit(rest[digits])) {
    ++digits;
  }

  if (digits == 0 || digits >= rest.size() || digits > 9) {
    return match;
  }

  const char delimiter = rest[digits];

  if (delimiter != '.' && delimiter != ')') {
    return match;
  }

  match.matched = true;
  match.ordered = true;
  match.start = std::stoi(std::string(rest.substr(0, digits)));
  match.delimiter = delimiter == '.' ? ListDelimiter::period : ListDelimiter::paren;
  match.marker_width = digits + 1;

  return match;
}

const std::vector<std::string>& html_block_tags() {
  static const std::vector<std::string> tags{
    "address", "article", "aside", "base", "basefont", "blockquote", "body", "caption", "center", "col",
    "colgroup", "dd", "details", "dialog", "dir", "div", "dl", "dt", "fieldset", "figcaption", "figure",
    "footer", "form", "frame", "frameset", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hr",
    "html", "iframe", "legend", "li", "link", "main", "menu", "menuitem", "nav", "noframes", "ol",
    "optgroup", "option", "p", "param", "search", "section", "summary", "table", "tbody", "td", "tfoot",
    "th", "thead", "title", "tr", "track", "ul",
  };

  return tags;
}

int match_html_block_start(std::string_view rest, bool in_paragraph) {
  if (rest.empty() || rest[0] != '<') {
    return 0;
  }

  const std::string_view after = rest.substr(1);
  const std::string lowered_after = text::to_lower_ascii(after);

  if (lowered_after.starts_with("script") || lowered_after.starts_with("pre") ||
      lowered_after.starts_with("style") || lowered_after.starts_with("textarea")) {
    return 1;
  }

  if (after.starts_with("!--")) {
    return 2;
  }

  if (after.starts_with("?")) {
    return 3;
  }

  if (after.starts_with("![CDATA[")) {
    return 5;
  }

  if (after.size() > 1 && after[0] == '!' && is_letter(after[1])) {
    return 4;
  }

  std::string_view name = after;

  if (name.starts_with('/')) {
    name.remove_prefix(1);
  }

  std::size_t length = 0;

  while (length < name.size() && (is_letter(name[length]) || is_digit(name[length]))) {
    ++length;
  }

  if (length == 0) {
    return 0;
  }

  const std::string tag = text::to_lower_ascii(name.substr(0, length));
  const std::string_view tail = name.substr(length);
  const auto& tags = html_block_tags();

  if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
    if (tail.empty() || is_space_or_tab(tail[0]) || tail.starts_with('>') || tail.starts_with("/>")) {
      return 6;
    }
  }

  // Any other kind of tag, but only a real tag, only when it stands alone on
  // its line, and never interrupting a paragraph. Without the tag grammar an
  // autolink such as <http://example.com> would be read as a block.
  if (!in_paragraph) {
    if (const std::size_t tag_length = match_html_tag(rest); tag_length > 0) {
      if (text::trim(rest.substr(tag_length)).empty()) {
        return 7;
      }
    }
  }

  return 0;
}

bool html_block_ends(int type, std::string_view line) {
  const std::string lowered = text::to_lower_ascii(line);

  switch (type) {
    case 1:
      return lowered.contains("</script>") || lowered.contains("</pre>") ||
             lowered.contains("</style>") || lowered.contains("</textarea>");
    case 2:
      return line.contains("-->");
    case 3:
      return line.contains("?>");
    case 4:
      return line.contains('>');
    case 5:
      return line.contains("]]>");
    default:
      return false;
  }
}

// A line of only equals signs or only hyphens underlines the paragraph above
// it. Returns the heading level, or zero when the line is not an underline.
int setext_level(std::string_view rest) {
  const std::string_view trimmed = text::trim_end(rest);

  if (trimmed.empty() || (trimmed[0] != '=' && trimmed[0] != '-')) {
    return 0;
  }

  if (trimmed.find_first_not_of(trimmed[0]) != std::string_view::npos) {
    return 0;
  }

  return trimmed[0] == '=' ? 1 : 2;
}

// A table's delimiter row is what turns the line above it into a header. Each
// cell is a run of hyphens, optionally anchored with colons to set alignment.
std::vector<std::string_view> split_table_row(std::string_view line) {
  std::string_view trimmed = text::trim(line);

  if (trimmed.starts_with('|')) {
    trimmed.remove_prefix(1);
  }

  if (trimmed.ends_with('|') && !trimmed.ends_with("\\|")) {
    trimmed.remove_suffix(1);
  }

  std::vector<std::string_view> cells;
  std::size_t start = 0;

  for (std::size_t index = 0; index <= trimmed.size(); ++index) {
    if (index < trimmed.size() && trimmed[index] == '\\') {
      ++index;
      continue;
    }

    if (index == trimmed.size() || trimmed[index] == '|') {
      cells.push_back(text::trim(trimmed.substr(start, index - start)));
      start = index + 1;
    }
  }

  return cells;
}

std::vector<CellAlignment> table_alignments(std::string_view line) {
  std::vector<CellAlignment> alignments;

  if (!line.contains('-')) {
    return alignments;
  }

  for (const std::string_view cell : split_table_row(line)) {
    if (cell.empty()) {
      return {};
    }

    const bool left = cell.front() == ':';
    const bool right = cell.back() == ':';

    std::string_view dashes = cell;

    if (left) {
      dashes.remove_prefix(1);
    }

    if (right && !dashes.empty()) {
      dashes.remove_suffix(1);
    }

    if (dashes.empty() || dashes.find_first_not_of('-') != std::string_view::npos) {
      return {};
    }

    if (left && right) {
      alignments.push_back(CellAlignment::center);
    } else if (left) {
      alignments.push_back(CellAlignment::left);
    } else if (right) {
      alignments.push_back(CellAlignment::right);
    } else {
      alignments.push_back(CellAlignment::none);
    }
  }

  return alignments;
}

// A task marker opens the first paragraph of a list item.
struct TaskMatch {
  bool matched = false;
  bool checked = false;
  std::size_t width = 0;
};

TaskMatch match_task_marker(std::string_view content) {
  TaskMatch match;

  if (content.size() < 3 || content[0] != '[' || content[2] != ']') {
    return match;
  }

  const char box = content[1];

  if (box != ' ' && box != 'x' && box != 'X') {
    return match;
  }

  if (content.size() > 3 && !is_space_or_tab(content[3])) {
    return match;
  }

  match.matched = true;
  match.checked = box != ' ';
  match.width = content.size() > 3 ? 4 : 3;

  return match;
}

// {{< name key="value" >}} alone on a line.
struct ShortcodeMatch {
  bool matched = false;
  std::string_view name;
  std::string_view arguments;
};

ShortcodeMatch match_shortcode(std::string_view rest) {
  ShortcodeMatch match;

  const std::string_view trimmed = text::trim(rest);

  if (!trimmed.starts_with("{{<") || !trimmed.ends_with(">}}")) {
    return match;
  }

  std::string_view body = text::trim(trimmed.substr(3, trimmed.size() - 6));
  std::size_t length = 0;

  while (length < body.size() && (is_letter(body[length]) || is_digit(body[length]) || body[length] == '-' ||
                                  body[length] == '_')) {
    ++length;
  }

  if (length == 0) {
    return match;
  }

  match.matched = true;
  match.name = body.substr(0, length);
  match.arguments = text::trim(body.substr(length));

  return match;
}

// ": definition" beneath a line of text makes a definition list.
bool is_definition_line(std::string_view rest) {
  const std::string_view trimmed = text::trim_start(rest);

  return trimmed.size() > 1 && trimmed[0] == ':' && is_space_or_tab(trimmed[1]);
}

class BlockParser {
public:
  BlockParser(Arena& arena, std::string_view input) : arena_(arena), input_(input) {}

  Node* parse() {
    document_ = make(NodeKind::document);
    tip_ = document_;

    for (const std::string_view raw : text::split_lines(input_)) {
      handle_line(raw);
    }

    while (tip_ != nullptr && tip_ != document_) {
      Node* current = tip_;
      close(current);

      if (tip_ == current) {
        break;
      }
    }

    close_remaining(document_);

    // Inlines are parsed once the block structure has settled, because a
    // reference defined at the end of a document is visible to a link at the
    // start of it.
    parse_inlines(arena_, document_, references_);

    // Footnotes are numbered by where they are referred to, so this waits until
    // every reference in the document exists.
    if (Node* section = resolve_footnotes(arena_, document_, footnotes_, references_)) {
      append_child(document_, section);
    }

    return document_;
  }

private:
  // Closes what is still open, deepest first, without walking the tip pointer
  // above the document.
  void close_remaining(Node* node) {
    for (Node* child = node->first_child; child != nullptr; child = child->next_sibling) {
      close_remaining(child);
    }

    if (node->open && node != document_) {
      close(node);
    }

    if (node == document_) {
      node->open = false;
    }
  }

  Node* make(NodeKind kind) {
    Node* node = arena_.create<Node>();
    node->kind = kind;

    return node;
  }

  std::string& content_of(Node* node) { return content_[node]; }

  static bool accepts_lines(const Node* node) {
    return node->kind == NodeKind::paragraph || node->kind == NodeKind::code_block ||
           node->kind == NodeKind::html_block || node->kind == NodeKind::table;
  }

  static bool is_container(const Node* node) {
    return node->kind == NodeKind::document || node->kind == NodeKind::block_quote ||
           node->kind == NodeKind::list || node->kind == NodeKind::item ||
           node->kind == NodeKind::definition_list;
  }

  void close(Node* node) {
    if (!node->open) {
      return;
    }

    node->open = false;

    if (node->kind == NodeKind::paragraph) {
      finalize_paragraph(node);
    } else if (node->kind == NodeKind::code_block) {
      std::string body = content_of(node);

      // An indented block's trailing blank lines are indentation, not content.
      if (!node->fenced) {
        std::size_t end = body.size();

        while (end > 0) {
          const std::size_t line_start = body.find_last_of('\n', end - 1) == std::string::npos
                                           ? 0
                                           : body.find_last_of('\n', end - 1) + 1;
          const std::string_view line(body.data() + line_start, end - line_start);

          if (!is_blank(line)) {
            break;
          }

          end = line_start == 0 ? 0 : line_start - 1;
        }

        body = end == 0 ? std::string{} : body.substr(0, end) + "\n";
      }

      node->literal = arena_.intern(body);
      content_.erase(node);
    } else if (node->kind == NodeKind::table) {
      finalize_table(node);
    } else if (node->kind == NodeKind::html_block || node->kind == NodeKind::heading) {
      node->literal = arena_.intern(content_of(node));
      content_.erase(node);
    } else if (node->kind == NodeKind::list) {
      finalize_list(node);
    }

    tip_ = node->parent != nullptr ? node->parent : document_;
  }

  // A paragraph may open with link reference definitions. They are not content,
  // so they come out before anything else reads the text.
  void finalize_paragraph(Node* node) {
    std::string body = content_of(node);
    std::string_view remaining = body;

    while (!remaining.empty()) {
      if (const std::size_t consumed = parse_footnote_definition(arena_, remaining, footnotes_); consumed > 0) {
        remaining = remaining.substr(consumed);
        continue;
      }

      const std::size_t consumed = parse_reference_definition(arena_, remaining, references_);

      if (consumed == 0) {
        break;
      }

      remaining = remaining.substr(consumed);
    }

    content_.erase(node);

    if (text::trim(remaining).empty()) {
      detach(node);

      return;
    }

    node->literal = arena_.intern(text::trim(remaining));
  }

  // The caller copies the heading line out of the content map first. This
  // erases the entry that held it, so a reference into the map would dangle.
  void start_table(const std::string& heading_line, const std::vector<CellAlignment>& alignments) {
    Node* paragraph = tip_;
    content_.erase(paragraph);
    detach(paragraph);
    tip_ = paragraph->parent != nullptr ? paragraph->parent : document_;

    Node* table = add_child(NodeKind::table);
    alignments_ = alignments;
    content_of(table) = heading_line + "\n";
    header_pending_ = true;
  }

  void finalize_table(Node* table) {
    const std::string body = content_of(table);
    content_.erase(table);

    bool first = true;

    for (const std::string_view line : text::split_lines(body)) {
      if (text::trim(line).empty()) {
        continue;
      }

      Node* row = arena_.create<Node>();
      row->kind = NodeKind::table_row;
      row->header = first;
      append_child(table, row);

      const std::vector<std::string_view> cells = split_table_row(line);

      // A row is padded or truncated to the width the delimiter row declared.
      for (std::size_t index = 0; index < alignments_.size(); ++index) {
        Node* cell = arena_.create<Node>();
        cell->kind = NodeKind::table_cell;
        cell->header = first;
        cell->alignment = alignments_[index];
        cell->literal = index < cells.size() ? arena_.intern(unescape_pipes(cells[index])) : std::string_view{};

        append_child(row, cell);
      }

      first = false;
    }
  }

  static std::string unescape_pipes(std::string_view cell) {
    std::string out;

    for (std::size_t index = 0; index < cell.size(); ++index) {
      if (cell[index] == '\\' && index + 1 < cell.size() && cell[index + 1] == '|') {
        out += '|';
        ++index;
        continue;
      }

      out += cell[index];
    }

    return out;
  }

  // The paragraph above the first ": " becomes the term of a definition list.
  void start_definition_list() {
    Node* paragraph = tip_;
    const std::string term_text = content_of(paragraph);
    content_.erase(paragraph);
    detach(paragraph);
    tip_ = paragraph->parent != nullptr ? paragraph->parent : document_;

    if (tip_->kind == NodeKind::definition_list) {
      Node* term = arena_.create<Node>();
      term->kind = NodeKind::definition_term;
      term->literal = arena_.intern(term_text);
      term->open = false;
      append_child(tip_, term);

      return;
    }

    Node* list = add_child(NodeKind::definition_list);

    Node* term = arena_.create<Node>();
    term->kind = NodeKind::definition_term;
    term->literal = arena_.intern(term_text);
    term->open = false;
    append_child(list, term);
  }

  void add_definition_detail(std::string_view body) {
    Node* list = tip_;

    while (list != nullptr && list->kind != NodeKind::definition_list) {
      list = list->parent;
    }

    if (list == nullptr) {
      return;
    }

    Node* detail = arena_.create<Node>();
    detail->kind = NodeKind::definition_detail;
    detail->literal = arena_.intern(body);
    detail->open = false;
    append_child(list, detail);

    tip_ = list;
  }

  static void detach(Node* node) {
    Node* parent = node->parent;

    if (parent == nullptr) {
      return;
    }

    Node* previous = nullptr;

    for (Node* child = parent->first_child; child != nullptr && child != node; child = child->next_sibling) {
      previous = child;
    }

    if (previous == nullptr) {
      parent->first_child = node->next_sibling;
    } else {
      previous->next_sibling = node->next_sibling;
    }

    if (parent->last_child == node) {
      parent->last_child = previous;
    }

    node->parent = nullptr;
  }

  static void loosen(Node* list) {
    if (list != nullptr && list->kind == NodeKind::list) {
      list->tight = false;
    }
  }

  // A list is loose when a blank line separates content inside it.
  static void finalize_list(Node* list) {
    for (Node* item = list->first_child; item != nullptr; item = item->next_sibling) {
      if (item->last_line_blank && item->next_sibling != nullptr) {
        list->tight = false;

        return;
      }

      for (Node* child = item->first_child; child != nullptr; child = child->next_sibling) {
        if (child->last_line_blank && (child->next_sibling != nullptr || item->next_sibling != nullptr)) {
          list->tight = false;

          return;
        }
      }
    }
  }

  Node* add_child(NodeKind kind) {
    if (tip_ == nullptr) {
      tip_ = document_;
    }

    while (tip_ != nullptr && tip_ != document_ && !is_container(tip_)) {
      close(tip_);
    }

    if (tip_ == nullptr) {
      tip_ = document_;
    }

    // Only items belong directly to a list. Anything else means the list has
    // ended.
    if (kind != NodeKind::item && tip_->kind == NodeKind::list) {
      close(tip_);
    }

    if (tip_ == nullptr) {
      tip_ = document_;
    }

    // A blank line inside an item, followed by more content in that item, makes
    // the whole list loose. So does a blank line between two items.
    if (tip_->kind == NodeKind::item && tip_->last_line_blank && tip_->first_child != nullptr) {
      loosen(tip_->parent);
    }

    if (kind == NodeKind::item && tip_->kind == NodeKind::list && tip_->last_child != nullptr &&
        tip_->last_child->last_line_blank) {
      loosen(tip_);
    }

    Node* node = make(kind);
    append_child(tip_, node);
    tip_ = node;

    return node;
  }

  void handle_line(std::string_view raw) {
    Scanner scanner(raw);
    line_consumed_ = false;

    Node* container = document_;
    bool all_matched = true;

    Node* child = open_child(container);

    while (child != nullptr) {
      container = child;

      // A container that fails to match must leave the scanner where it found
      // it. Otherwise the columns it consumed on the way to failing are lost,
      // and every indentation measured afterward starts from the wrong origin.
      const Scanner before = scanner;

      if (!continues(container, scanner)) {
        if (line_consumed_) {
          return;
        }

        scanner = before;
        container = container->parent;
        all_matched = false;
        break;
      }

      child = open_child(container);
    }

    if (container == nullptr) {
      container = document_;
    }

    const bool blank = is_blank(scanner.rest());

    if (!all_matched) {
      // A paragraph left open by the previous line swallows a continuation that
      // starts no new block, even though the containers around it did not
      // match. This is what lets a quoted paragraph run past its own marker.
      if (tip_ != nullptr && tip_->kind == NodeKind::paragraph && !blank && starts_no_block(scanner)) {
        add_text(tip_, scanner.rest());

        return;
      }

      while (tip_ != nullptr && tip_ != container && tip_ != document_) {
        close(tip_);
      }
    } else {
      tip_ = container;
    }

    // The descent stops on the deepest block that still matched, which for a
    // code or html block is the block itself rather than one of its children.
    if (all_matched && accepts_lines(container) && container->kind != NodeKind::paragraph) {
      tip_ = container;
      add_text(container, scanner.rest());

      return;
    }

    if (container->kind == NodeKind::paragraph) {
      tip_ = container;
    }

    open_new_blocks(scanner);
  }

  static Node* open_child(Node* parent) {
    if (parent == nullptr) {
      return nullptr;
    }

    Node* child = parent->last_child;

    return child != nullptr && child->open ? child : nullptr;
  }

  static bool starts_no_block(const Scanner& scanner) {
    const std::string_view raw = scanner.rest();

    // Four columns of indentation would mean indented code, and indented code
    // cannot interrupt a paragraph, so the line continues the paragraph
    // whatever it looks like after the indentation.
    int indent = 0;

    for (const char character : raw) {
      if (character == ' ') {
        ++indent;
      } else if (character == '\t') {
        indent += 4 - (indent % 4);
      } else {
        break;
      }

      if (indent >= code_indent) {
        return true;
      }
    }

    const std::string_view rest = text::trim_start(raw);

    if (rest.empty() || rest[0] == '>') {
      return false;
    }

    if (matches_thematic_break(rest) || match_atx_heading(rest).matched || match_code_fence(rest).matched) {
      return false;
    }

    if (match_html_block_start(rest, true) > 0) {
      return false;
    }

    if (const ListMatch list = match_list_marker(rest); list.matched) {
      const std::string_view after = rest.substr(list.marker_width);

      // A marker alone on its line opens an empty item. Whether that is allowed
      // to interrupt a paragraph is decided where blocks are opened, not here.
      if (after.empty() || is_space_or_tab(after[0])) {
        return false;
      }
    }

    return true;
  }

  bool continues(Node* node, Scanner& scanner) {
    switch (node->kind) {
      case NodeKind::block_quote: {
        const int indent = scanner.skip_spaces(3);

        if (indent <= 3 && scanner.peek() == '>') {
          scanner.advance();

          if (is_space_or_tab(scanner.peek())) {
            scanner.advance();
          }

          return true;
        }

        return false;
      }

      case NodeKind::item: {
        if (is_blank(scanner.rest())) {
          return node->first_child != nullptr;
        }

        const int started = scanner.column;
        scanner.skip_spaces(node->list_padding);

        return scanner.column - started >= node->list_padding;
      }

      case NodeKind::code_block: {
        if (node->fenced) {
          const Scanner saved = scanner;
          const int indent = scanner.skip_spaces(3);

          if (indent <= 3 && matches_closing_fence(scanner.rest(), node->fence_char, node->fence_length)) {
            close(node);

            // The fence line is the closer and nothing else. Without this the
            // same line would go on to open a fresh code block.
            line_consumed_ = true;

            return false;
          }

          scanner = saved;
          scanner.skip_spaces(node->fence_offset);

          return true;
        }

        if (is_blank(scanner.rest())) {
          scanner.skip_spaces(code_indent);

          return true;
        }

        const int started = scanner.column;
        scanner.skip_spaces(code_indent);

        return scanner.column - started >= code_indent;
      }

      case NodeKind::html_block:
        if (node->html_block_type >= 6 && is_blank(scanner.rest())) {
          close(node);

          return false;
        }

        return true;

      case NodeKind::definition_list:
        return !is_blank(scanner.rest());

      case NodeKind::table:
        return !is_blank(scanner.rest()) && scanner.rest().contains('|');

      case NodeKind::paragraph:
        return !is_blank(scanner.rest());

      default:
        return true;
    }
  }

  void open_new_blocks(Scanner& scanner) {
    int depth = 0;

    while (depth++ < max_nesting) {
      const Scanner before_indent = scanner;
      const int indent = scanner.skip_spaces(100);
      std::string_view rest = scanner.rest();

      if (indent >= code_indent) {
        if (is_blank(rest) || in_paragraph()) {
          scanner = before_indent;
          break;
        }

        Scanner code_scanner = before_indent;
        code_scanner.skip_spaces(code_indent);

        Node* code = add_child(NodeKind::code_block);
        content_of(code) = std::string(code_scanner.rest()) + "\n";

        return;
      }

      if (rest.empty()) {
        break;
      }

      if (rest[0] == '>') {
        scanner.advance();

        if (is_space_or_tab(scanner.peek())) {
          scanner.advance();
        }

        close_paragraph_before_new_block();
        add_child(NodeKind::block_quote);

        continue;
      }

      if (const ShortcodeMatch shortcode = match_shortcode(rest); shortcode.matched) {
        close_paragraph_before_new_block();

        Node* node = add_child(NodeKind::shortcode);
        node->label = arena_.intern(shortcode.name);
        node->raw = arena_.intern(shortcode.arguments);
        node->literal = arena_.intern(text::trim(rest));
        close(node);

        return;
      }

      // A definition line beneath an open paragraph turns it into a term.
      if (in_paragraph() && is_definition_line(rest) && !content_of(tip_).empty()) {
        start_definition_list();
        add_definition_detail(text::trim(text::trim_start(rest).substr(1)));

        return;
      }

      if (const AtxMatch heading = match_atx_heading(rest); heading.matched) {
        close_paragraph_before_new_block();

        Node* node = add_child(NodeKind::heading);
        node->level = heading.level;
        content_of(node) = std::string(heading.content);
        close(node);

        return;
      }

      if (const FenceMatch fence = match_code_fence(rest); fence.matched) {
        close_paragraph_before_new_block();

        Node* code = add_child(NodeKind::code_block);
        code->fenced = true;
        code->fence_char = fence.character;
        code->fence_length = fence.length;
        code->fence_offset = indent;
        code->info = arena_.intern(unescape_string(fence.info));
        content_of(code) = "";

        return;
      }

      // A delimiter row beneath a single-line paragraph turns that line into a
      // table header. Checked before the setext underline, since a row of
      // hyphens with pipes in it is a table rather than a heading.
      if (in_paragraph() && rest.contains('|')) {
        const std::string heading_line = content_of(tip_);

        if (!heading_line.contains('\n')) {
          const std::vector<CellAlignment> alignments = table_alignments(rest);

          if (!alignments.empty() && alignments.size() == split_table_row(heading_line).size()) {
            start_table(heading_line, alignments);

            return;
          }
        }
      }

      // An underline beneath an open paragraph makes it a heading, and takes
      // precedence over reading the same characters as a thematic break.
      if (in_paragraph() && !content_of(tip_).empty()) {
        if (const int level = setext_level(rest); level > 0) {
          Node* heading = tip_;
          heading->kind = NodeKind::heading;
          heading->level = level;

          const std::string body = content_of(heading);
          content_of(heading) = std::string(text::trim(body));

          close(heading);

          return;
        }
      }

      if (matches_thematic_break(rest)) {
        close_paragraph_before_new_block();

        Node* rule = add_child(NodeKind::thematic_break);
        close(rule);

        return;
      }

      if (const int html_type = match_html_block_start(rest, in_paragraph()); html_type > 0) {
        close_paragraph_before_new_block();

        Node* html = add_child(NodeKind::html_block);
        html->html_block_type = html_type;
        content_of(html) = std::string(rest) + "\n";

        if (html_type <= 5 && html_block_ends(html_type, rest)) {
          close(html);
        }

        return;
      }

      if (const ListMatch list = match_list_marker(rest); list.matched) {
        const std::string_view after = rest.substr(list.marker_width);
        const bool empty_item = text::trim(after).empty();

        if (!after.empty() && !is_space_or_tab(after[0])) {
          break;
        }

        // A list interrupts a paragraph only when it starts at one and carries
        // content.
        if (in_paragraph() && (empty_item || (list.ordered && list.start != 1))) {
          break;
        }

        for (std::size_t skipped = 0; skipped < list.marker_width; ++skipped) {
          scanner.advance();
        }

        int padding = 1;

        if (!empty_item) {
          const Scanner before_padding = scanner;
          const int measured = scanner.skip_spaces(5);

          if (measured >= 1 && measured <= 4) {
            padding = measured;
          } else {
            scanner = before_padding;
            scanner.skip_spaces(1);
            padding = 1;
          }
        }

        const int item_indent = indent + static_cast<int>(list.marker_width) + padding;

        close_paragraph_before_new_block();

        if (tip_ == nullptr || tip_->kind != NodeKind::list ||
            tip_->ordered != list.ordered ||
            (list.ordered && tip_->delimiter != list.delimiter) ||
            (!list.ordered && std::cmp_not_equal(tip_->level ,static_cast<unsigned char>(list.bullet)))) {
          Node* container = add_child(NodeKind::list);
          container->ordered = list.ordered;
          container->start = list.start;
          container->delimiter = list.delimiter;
          container->level = list.ordered ? 0 : static_cast<int>(static_cast<unsigned char>(list.bullet));
        }

        Node* item = add_child(NodeKind::item);
        item->list_padding = item_indent;

        continue;
      }

      break;
    }

    if (is_blank(scanner.rest())) {
      mark_blank();

      return;
    }

    if (tip_ != nullptr && tip_->kind == NodeKind::definition_list) {
      const std::string_view rest = scanner.rest();

      if (is_definition_line(rest)) {
        add_definition_detail(text::trim(text::trim_start(rest).substr(1)));

        return;
      }

      Node* term = arena_.create<Node>();
      term->kind = NodeKind::definition_term;
      term->literal = arena_.intern(text::trim(rest));
      term->open = false;
      append_child(tip_, term);

      return;
    }

    if (in_paragraph()) {
      add_text(tip_, scanner.rest());

      return;
    }

    std::string_view body = text::trim_start(scanner.rest());

    if (tip_ != nullptr && tip_->kind == NodeKind::item && tip_->first_child == nullptr) {
      if (const TaskMatch task = match_task_marker(body); task.matched) {
        tip_->task = true;
        tip_->checked = task.checked;
        body = text::trim_start(body.substr(task.width));
      }
    }

    Node* paragraph = add_child(NodeKind::paragraph);
    content_of(paragraph) = std::string(body);
  }

  void close_paragraph_before_new_block() {
    if (in_paragraph()) {
      close(tip_);
    }
  }

  bool in_paragraph() const { return tip_ != nullptr && tip_->kind == NodeKind::paragraph; }

  void mark_blank() {
    if (in_paragraph()) {
      close(tip_);
    }

    for (Node* walk = tip_; walk != nullptr; walk = walk->parent) {
      walk->last_line_blank = true;
    }
  }

  void add_text(Node* node, std::string_view line) {
    std::string& body = content_of(node);

    if (node->kind == NodeKind::code_block || node->kind == NodeKind::html_block) {
      body += std::string(line);
      body += "\n";

      if (node->kind == NodeKind::html_block && node->html_block_type <= 5 &&
          html_block_ends(node->html_block_type, line)) {
        close(node);
      }

      return;
    }

    if (!body.empty()) {
      body += "\n";
    }

    body += std::string(text::trim_start(line));
  }

  Arena& arena_;
  std::string_view input_;
  std::vector<CellAlignment> alignments_;
  bool header_pending_ = false;
  bool line_consumed_ = false;
  Node* document_ = nullptr;
  Node* tip_ = nullptr;
  std::unordered_map<Node*, std::string> content_;
  ReferenceMap references_;
  FootnoteMap footnotes_;
};

}  // namespace

void append_child(Node* parent, Node* child) {
  child->parent = parent;

  if (parent->last_child == nullptr) {
    parent->first_child = child;
    parent->last_child = child;

    return;
  }

  parent->last_child->next_sibling = child;
  parent->last_child = child;
}

Node* parse_markdown(Arena& arena, std::string_view input) {
  BlockParser parser(arena, input);

  return parser.parse();
}

Node* parse_markdown(Arena& arena, const Source& source) {
  return parse_markdown(arena, source.view());
}

}  // namespace blogin
