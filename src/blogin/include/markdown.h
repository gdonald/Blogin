#pragma once

#include <string>
#include <string_view>

#include "arena.h"

namespace blogin {

enum class NodeKind {
  document,

  // Containers.
  block_quote,
  list,
  item,

  // Leaf blocks.
  paragraph,
  heading,
  code_block,
  html_block,
  thematic_break,
  table,
  table_row,
  table_cell,
  definition_list,
  definition_term,
  definition_detail,
  footnotes,
  footnote_item,
  shortcode,

  // Inlines.
  text,
  soft_break,
  line_break,
  code_span,
  html_inline,
  emphasis,
  strong,
  strikethrough,
  link,
  image,
  footnote_ref,
  math,
};

enum class CellAlignment {
  none,
  left,
  center,
  right,
};

enum class ListDelimiter {
  none,
  period,
  paren,
};

// An attribute written in a trailing {.class #id key=value} block. Arena
// allocated and chained, so a Node stays trivially destructible.
struct Attribute {
  std::string_view name;
  std::string_view value;
  Attribute* next = nullptr;
};

// One node of the parse tree, arena-allocated with intrusive links.
//
// Text-bearing nodes hold a view into either the source buffer or the arena.
// Both outlive the tree, so a caller never has to know which.
struct Node {
  NodeKind kind = NodeKind::document;

  Node* parent = nullptr;
  Node* first_child = nullptr;
  Node* last_child = nullptr;
  Node* next_sibling = nullptr;

  // Heading level.
  int level = 0;

  // code_block, html_block, code_span, text
  std::string_view literal;

  // code_block info string
  std::string_view info;
  bool fenced = false;
  char fence_char = '\0';
  int fence_length = 0;
  int fence_offset = 0;

  // link and image
  std::string_view url;
  std::string_view title;

  // list
  bool ordered = false;
  bool tight = true;
  int start = 1;
  ListDelimiter delimiter = ListDelimiter::none;
  int list_marker_offset = 0;
  int list_padding = 0;

  // item
  bool task = false;
  bool checked = false;

  // table_row and table_cell
  bool header = false;
  CellAlignment alignment = CellAlignment::none;

  // link and image
  Attribute* attributes = nullptr;

  // footnote_ref and footnote_item
  std::string_view label;
  int number = 0;
  int occurrence = 1;

  // math
  bool display = false;

  // shortcode
  std::string_view raw;

  // html_block
  int html_block_type = 0;

  bool open = true;
  bool last_line_blank = false;
};

// Owns the text a tree borrows from. The tree must not outlive it.
class Source {
public:
  explicit Source(std::string text) : text_(std::move(text)) {}

  std::string_view view() const { return text_; }

private:
  std::string text_;
};

Node* parse_markdown(Arena& arena, const Source& source);

Node* parse_markdown(Arena& arena, std::string_view input);

void append_child(Node* parent, Node* child);

}  // namespace blogin
