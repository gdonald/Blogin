#include <algorithm>
#include <string>
#include <vector>

#include "markdown_internal.h"
#include "text.h"

namespace blogin::markdown_detail {
namespace {

// A run of the same delimiter character, recorded so emphasis can be resolved
// after the whole line has been scanned.
struct Delimiter {
  Node* node = nullptr;
  char character = '\0';
  int count = 0;
  int original_count = 0;
  bool can_open = false;
  bool can_close = false;
  bool active = true;
  std::size_t index = 0;
};

// A bracket waiting for its closing counterpart.
struct Bracket {
  Node* node = nullptr;
  std::size_t position = 0;
  std::size_t delimiter_count = 0;
  bool image = false;
  bool active = true;
};

// Matches an open or closing tag against the specification's grammar. Looking
// for the next '>' is not enough: an attribute value may quote a '>' or a '<',
// and an attribute name that is not a name at all makes the whole thing text.
std::size_t match_tag_impl(std::string_view rest) {
  std::size_t index = 1;

  const bool closing_tag = index < rest.size() && rest[index] == '/';

  if (closing_tag) {
    ++index;
  }

  if (index >= rest.size() || !is_letter(rest[index])) {
    return 0;
  }

  while (index < rest.size() && (is_letter(rest[index]) || is_digit(rest[index]) || rest[index] == '-')) {
    ++index;
  }

  auto skip_whitespace = [&] {
    const std::size_t started = index;

    while (index < rest.size() && is_whitespace(rest[index])) {
      ++index;
    }

    return index - started;
  };

  if (closing_tag) {
    skip_whitespace();

    return index < rest.size() && rest[index] == '>' ? index + 1 : 0;
  }

  while (true) {
    const std::size_t spaces = skip_whitespace();

    if (index >= rest.size()) {
      return 0;
    }

    if (rest[index] == '>') {
      return index + 1;
    }

    if (rest[index] == '/') {
      ++index;

      return index < rest.size() && rest[index] == '>' ? index + 1 : 0;
    }

    // An attribute has to be separated from what came before it.
    if (spaces == 0) {
      return 0;
    }

    if (!is_letter(rest[index]) && rest[index] != '_' && rest[index] != ':') {
      return 0;
    }

    while (index < rest.size() && (is_letter(rest[index]) || is_digit(rest[index]) || rest[index] == '_' ||
                                   rest[index] == '.' || rest[index] == ':' || rest[index] == '-')) {
      ++index;
    }

    const std::size_t before_equals = index;
    skip_whitespace();

    if (index >= rest.size() || rest[index] != '=') {
      index = before_equals;
      continue;
    }

    ++index;
    skip_whitespace();

    if (index >= rest.size()) {
      return 0;
    }

    if (rest[index] == '"' || rest[index] == '\'') {
      const char quote = rest[index];
      const auto closing_quote = rest.find(quote, index + 1);

      if (closing_quote == std::string_view::npos) {
        return 0;
      }

      index = closing_quote + 1;
      continue;
    }

    const std::size_t value_start = index;

    while (index < rest.size() && !is_whitespace(rest[index]) && rest[index] != '"' && rest[index] != '\'' &&
           rest[index] != '=' && rest[index] != '<' && rest[index] != '>' && rest[index] != '`') {
      ++index;
    }

    if (index == value_start) {
      return 0;
    }
  }
}

class InlineParser {
public:
  InlineParser(Arena& arena, std::string_view input, const ReferenceMap& references)
    : arena_(arena), input_(input), references_(references) {}

  void parse_into(Node* parent) {
    parent_ = parent;

    while (position_ < input_.size()) {
      step();
    }

    flush_text();
    process_emphasis(0);

    for (Node* node : nodes_) {
      append_child(parent, node);
    }
  }

private:
  Node* make(NodeKind kind) {
    Node* node = arena_.create<Node>();
    node->kind = kind;

    return node;
  }

  void flush_text() {
    if (pending_.empty()) {
      return;
    }

    Node* node = make(NodeKind::text);
    node->literal = arena_.intern(pending_);
    nodes_.push_back(node);
    pending_.clear();
  }

  void emit(Node* node) {
    flush_text();
    nodes_.push_back(node);
  }

  char peek(std::size_t ahead = 0) const {
    return position_ + ahead < input_.size() ? input_[position_ + ahead] : '\0';
  }

  void step() {
    const char character = peek();

    switch (character) {
      case '\\':
        handle_backslash();
        return;
      case '`':
        handle_code_span();
        return;
      case '&':
        handle_entity();
        return;
      case '<':
        handle_angle();
        return;
      case '[':
        if (peek(1) == '^' && scan_footnote_reference()) {
          return;
        }

        handle_open_bracket(false);
        return;

      case '$':
        if (scan_math()) {
          return;
        }

        break;
      case '!':
        if (peek(1) == '[') {
          handle_open_bracket(true);
          return;
        }
        break;
      case ']':
        handle_close_bracket();
        return;
      case '*':
      case '_':
      case '~':
        handle_delimiter_run(character);
        return;
      case '\n':
        handle_newline();
        return;
      default:
        break;
    }

    pending_ += character;
    ++position_;
  }

  void handle_backslash() {
    const char next = peek(1);

    if (next == '\n') {
      flush_text();
      nodes_.push_back(make(NodeKind::line_break));
      position_ += 2;

      return;
    }

    if (is_punctuation(next)) {
      pending_ += next;
      position_ += 2;

      return;
    }

    pending_ += '\\';
    ++position_;
  }

  void handle_newline() {
    // Two or more trailing spaces make a hard break.
    std::size_t trailing = 0;

    while (trailing < pending_.size() && pending_[pending_.size() - 1 - trailing] == ' ') {
      ++trailing;
    }

    pending_.resize(pending_.size() - trailing);

    flush_text();
    nodes_.push_back(make(trailing >= 2 ? NodeKind::line_break : NodeKind::soft_break));

    ++position_;

    while (position_ < input_.size() && is_space_or_tab(input_[position_])) {
      ++position_;
    }
  }

  void handle_entity() {
    std::string decoded;

    if (const std::size_t consumed = decode_entity(input_.substr(position_), decoded); consumed > 0) {
      pending_ += decoded;
      position_ += consumed;

      return;
    }

    pending_ += '&';
    ++position_;
  }

  void handle_code_span() {
    std::size_t opening = 0;

    while (position_ + opening < input_.size() && input_[position_ + opening] == '`') {
      ++opening;
    }

    std::size_t search = position_ + opening;

    while (search < input_.size()) {
      if (input_[search] != '`') {
        ++search;
        continue;
      }

      std::size_t closing = 0;

      while (search + closing < input_.size() && input_[search + closing] == '`') {
        ++closing;
      }

      if (closing == opening) {
        std::string content(input_.substr(position_ + opening, search - position_ - opening));

        for (char& character : content) {
          if (character == '\n') {
            character = ' ';
          }
        }

        // One space either side is stripping, so that `` ` `` yields a backtick.
        if (content.size() >= 2 && content.front() == ' ' && content.back() == ' ' &&
            content.find_first_not_of(' ') != std::string::npos) {
          content = content.substr(1, content.size() - 2);
        }

        Node* node = make(NodeKind::code_span);
        node->literal = arena_.intern(content);
        emit(node);

        position_ = search + closing;

        return;
      }

      search += closing;
    }

    pending_.append(opening, '`');
    position_ += opening;
  }

  bool scan_autolink() {
    const auto closing = input_.find('>', position_);

    if (closing == std::string_view::npos) {
      return false;
    }

    const std::string_view body = input_.substr(position_ + 1, closing - position_ - 1);

    if (body.empty() || body.contains(' ') ||
        body.contains('<')) {
      return false;
    }

    const auto colon = body.find(':');
    bool is_uri = false;

    if (colon != std::string_view::npos && colon > 0 && colon <= 32) {
      is_uri = is_letter(body[0]);

      for (std::size_t index = 1; index < colon && is_uri; ++index) {
        const char character = body[index];

        is_uri = is_letter(character) || is_digit(character) || character == '+' || character == '.' ||
                 character == '-';
      }
    }

    const auto at_sign = body.find('@');
    const bool is_email = !is_uri && at_sign != std::string_view::npos && at_sign > 0 &&
                          at_sign + 1 < body.size() && body.find('@', at_sign + 1) == std::string_view::npos &&
                          body.find('.', at_sign) != std::string_view::npos;

    if (!is_uri && !is_email) {
      return false;
    }

    Node* link = make(NodeKind::link);
    link->url = arena_.intern(is_email ? "mailto:" + std::string(body) : std::string(body));

    Node* label = make(NodeKind::text);
    label->literal = arena_.intern(body);
    append_child(link, label);

    emit(link);
    position_ = closing + 1;

    return true;
  }

  bool scan_raw_html() {
    const std::string_view rest = input_.substr(position_);

    if (rest.size() < 3) {
      return false;
    }

    std::size_t length = 0;

    if (rest.starts_with("<!--")) {
      const auto closing = rest.find("-->", 4);

      if (closing == std::string_view::npos) {
        return false;
      }

      length = closing + 3;
    } else if (rest.starts_with("<?")) {
      const auto closing = rest.find("?>", 2);

      if (closing == std::string_view::npos) {
        return false;
      }

      length = closing + 2;
    } else if (rest.starts_with("<![CDATA[")) {
      const auto closing = rest.find("]]>", 9);

      if (closing == std::string_view::npos) {
        return false;
      }

      length = closing + 3;
    } else if (rest.size() > 2 && rest[1] == '!' && is_letter(rest[2])) {
      const auto closing = rest.find('>', 2);

      if (closing == std::string_view::npos) {
        return false;
      }

      length = closing + 1;
    } else {
      length = match_html_tag(rest);

      if (length == 0) {
        return false;
      }
    }

    Node* node = make(NodeKind::html_inline);
    node->literal = arena_.intern(rest.substr(0, length));
    emit(node);

    position_ += length;

    return true;
  }

  void handle_angle() {
    if (scan_autolink() || scan_raw_html()) {
      return;
    }

    pending_ += '<';
    ++position_;
  }

  // [^label], recorded now and numbered once the whole document is parsed.
  bool scan_footnote_reference() {
    std::size_t scan = position_ + 2;

    while (scan < input_.size() && (is_letter(input_[scan]) || is_digit(input_[scan]) ||
                                    input_[scan] == '-' || input_[scan] == '_')) {
      ++scan;
    }

    if (scan == position_ + 2 || scan >= input_.size() || input_[scan] != ']') {
      return false;
    }

    Node* node = make(NodeKind::footnote_ref);
    node->label = arena_.intern(input_.substr(position_ + 2, scan - position_ - 2));

    emit(node);
    position_ = scan + 1;

    return true;
  }

  // $inline$ and $$display$$. A digit straight after the opening dollar means
  // it is a price, not mathematics.
  bool scan_math() {
    const bool display = peek(1) == '$';
    const std::size_t marker = display ? 2 : 1;
    const std::string_view closer = display ? "$$" : "$";

    if (!display) {
      const char next = peek(1);

      if (next == '\0' || is_whitespace(next) || is_digit(next)) {
        return false;
      }
    }

    const auto closing = input_.find(closer, position_ + marker);

    if (closing == std::string_view::npos || closing == position_ + marker) {
      return false;
    }

    const std::string_view body = input_.substr(position_ + marker, closing - position_ - marker);

    if (body.contains("\n\n")) {
      return false;
    }

    if (!display && is_whitespace(body.back())) {
      return false;
    }

    Node* node = make(NodeKind::math);
    node->display = display;
    node->literal = arena_.intern(body);

    emit(node);
    position_ = closing + marker;

    return true;
  }

  // A trailing {.class #id key=value} block attaches to the link or image just
  // closed.
  void attach_attribute_block(Node* node) {
    if (position_ >= input_.size() || input_[position_] != '{') {
      return;
    }

    const auto closing = input_.find('}', position_);

    if (closing == std::string_view::npos) {
      return;
    }

    const std::string_view body = input_.substr(position_ + 1, closing - position_ - 1);

    Attribute* first = nullptr;
    Attribute* last = nullptr;

    auto append = [&](std::string_view name, std::string_view value) {
      auto* attribute = arena_.create<Attribute>();
      attribute->name = arena_.intern(name);
      attribute->value = arena_.intern(value);

      if (last == nullptr) {
        first = attribute;
      } else {
        last->next = attribute;
      }

      last = attribute;
    };

    std::size_t index = 0;

    while (index < body.size()) {
      while (index < body.size() && is_whitespace(body[index])) {
        ++index;
      }

      if (index >= body.size()) {
        break;
      }

      if (body[index] == '.' || body[index] == '#') {
        const char kind = body[index];
        const std::size_t start = ++index;

        while (index < body.size() && !is_whitespace(body[index])) {
          ++index;
        }

        if (index > start) {
          append(kind == '.' ? "class" : "id", body.substr(start, index - start));
        }

        continue;
      }

      const std::size_t name_start = index;

      while (index < body.size() && body[index] != '=' && !is_whitespace(body[index])) {
        ++index;
      }

      const std::string_view name = body.substr(name_start, index - name_start);

      if (index >= body.size() || body[index] != '=' || name.empty()) {
        continue;
      }

      ++index;

      if (index < body.size() && (body[index] == '"' || body[index] == '\'')) {
        const char quote = body[index];
        const std::size_t value_start = ++index;

        while (index < body.size() && body[index] != quote) {
          ++index;
        }

        append(name, body.substr(value_start, index - value_start));

        if (index < body.size()) {
          ++index;
        }

        continue;
      }

      const std::size_t value_start = index;

      while (index < body.size() && !is_whitespace(body[index])) {
        ++index;
      }

      append(name, body.substr(value_start, index - value_start));
    }

    if (first != nullptr) {
      node->attributes = first;
      position_ = closing + 1;
    }
  }

  void handle_open_bracket(bool image) {
    const std::size_t marker_length = image ? 2 : 1;

    Node* node = make(NodeKind::text);
    node->literal = arena_.intern(input_.substr(position_, marker_length));

    emit(node);

    brackets_.push_back(Bracket{node, nodes_.size() - 1, delimiters_.size(), image, true});
    position_ += marker_length;
  }

  // Reads what follows a closing bracket: an inline destination, a reference
  // label, or nothing.
  bool resolve_link(std::string& url, std::string& title, bool& matched) {
    matched = false;

    if (position_ < input_.size() && input_[position_] == '(') {
      std::size_t scan = position_ + 1;

      while (scan < input_.size() && is_whitespace(input_[scan])) {
        ++scan;
      }

      std::string destination;

      if (scan < input_.size() && input_[scan] == '<') {
        // An angle-bracketed destination honours escapes and cannot run over a
        // line break, so the first '>' is not necessarily the closing one.
        std::size_t probe = scan + 1;
        std::string bracketed;
        bool closed = false;

        while (probe < input_.size()) {
          if (input_[probe] == '\\' && probe + 1 < input_.size() && is_punctuation(input_[probe + 1])) {
            bracketed += input_[probe + 1];
            probe += 2;
            continue;
          }

          if (input_[probe] == '\n' || input_[probe] == '<') {
            break;
          }

          if (input_[probe] == '>') {
            closed = true;
            break;
          }

          bracketed += input_[probe];
          ++probe;
        }

        if (!closed) {
          return false;
        }

        destination = bracketed;
        scan = probe + 1;
      } else {
        int depth = 0;

        while (scan < input_.size()) {
          const char character = input_[scan];

          if (character == '\\' && scan + 1 < input_.size() && is_punctuation(input_[scan + 1])) {
            destination += input_[scan + 1];
            scan += 2;
            continue;
          }

          if (is_whitespace(character)) {
            break;
          }

          if (character == '(') {
            ++depth;
          } else if (character == ')') {
            if (depth == 0) {
              break;
            }

            --depth;
          }

          destination += character;
          ++scan;
        }
      }

      while (scan < input_.size() && is_whitespace(input_[scan])) {
        ++scan;
      }

      std::string found_title;

      if (scan < input_.size() && (input_[scan] == '"' || input_[scan] == '\'' || input_[scan] == '(')) {
        const char opener = input_[scan];
        const char closer = opener == '(' ? ')' : opener;
        ++scan;

        while (scan < input_.size() && input_[scan] != closer) {
          if (input_[scan] == '\\' && scan + 1 < input_.size() && is_punctuation(input_[scan + 1])) {
            found_title += input_[scan + 1];
            scan += 2;
            continue;
          }

          found_title += input_[scan];
          ++scan;
        }

        if (scan >= input_.size()) {
          return false;
        }

        ++scan;
      }

      while (scan < input_.size() && is_whitespace(input_[scan])) {
        ++scan;
      }

      if (scan >= input_.size() || input_[scan] != ')') {
        return false;
      }

      url = unescape_string(destination);
      title = unescape_string(found_title);
      position_ = scan + 1;
      matched = true;

      return true;
    }

    return false;
  }

  bool lookup_reference(std::string_view label, std::string& url, std::string& title) {
    const auto found = references_.find(normalize_label(unescape_string(label)));

    if (found == references_.end()) {
      return false;
    }

    url = std::string(found->second.url);
    title = std::string(found->second.title);

    return true;
  }

  void handle_close_bracket() {
    ++position_;

    if (brackets_.empty()) {
      pending_ += ']';

      return;
    }

    Bracket bracket = brackets_.back();
    brackets_.pop_back();

    if (!bracket.active) {
      pending_ += ']';

      return;
    }

    const std::size_t text_start = bracket.position + 1;

    std::string url;
    std::string title;
    bool matched = false;

    resolve_link(url, title, matched);

    if (!matched) {
      // A reference link, either with an explicit label or using its own text.
      std::size_t scan = position_;
      std::string label;

      if (scan < input_.size() && input_[scan] == '[') {
        const auto closing = input_.find(']', scan);

        if (closing != std::string_view::npos) {
          label = std::string(input_.substr(scan + 1, closing - scan - 1));
          scan = closing + 1;
        }
      }

      if (label.empty()) {
        label = collect_text(text_start);
      }

      if (lookup_reference(label, url, title)) {
        matched = true;
        position_ = scan > position_ ? scan : position_;
      }
    }

    if (!matched) {
      pending_ += ']';

      return;
    }

    flush_text();

    Node* link = make(bracket.image ? NodeKind::image : NodeKind::link);
    link->url = arena_.intern(url);
    link->title = arena_.intern(title);

    process_emphasis(bracket.delimiter_count);

    for (std::size_t index = text_start; index < nodes_.size(); ++index) {
      append_child(link, nodes_[index]);
    }

    attach_attribute_block(link);

    nodes_.resize(bracket.position);
    nodes_.push_back(link);

    // A link cannot nest inside another link.
    if (!bracket.image) {
      for (Bracket& outer : brackets_) {
        if (!outer.image) {
          outer.active = false;
        }
      }
    }
  }

  std::string collect_text(std::size_t from) const {
    std::string out;

    for (std::size_t index = from; index < nodes_.size(); ++index) {
      if (nodes_[index]->kind == NodeKind::text || nodes_[index]->kind == NodeKind::code_span) {
        out += std::string(nodes_[index]->literal);
      }
    }

    return out + pending_;
  }

  void handle_delimiter_run(char character) {
    const std::size_t start = position_;

    while (position_ < input_.size() && input_[position_] == character) {
      ++position_;
    }

    const int count = static_cast<int>(position_ - start);

    const std::uint32_t before = start > 0 ? code_point_before(input_, start) : '\n';
    const std::uint32_t after = position_ < input_.size() ? code_point_at(input_, position_) : '\n';

    const bool before_whitespace = is_unicode_whitespace(before);
    const bool after_whitespace = is_unicode_whitespace(after);
    const bool before_punctuation = is_unicode_punctuation(before);
    const bool after_punctuation = is_unicode_punctuation(after);

    const bool left_flanking =
      !after_whitespace && (!after_punctuation || before_whitespace || before_punctuation);
    const bool right_flanking =
      !before_whitespace && (!before_punctuation || after_whitespace || after_punctuation);

    bool can_open = left_flanking;
    bool can_close = right_flanking;

    // A tilde run marks strikethrough, and only one or two of them do.
    if (character == '~' && count > 2) {
      can_open = false;
      can_close = false;
    }

    // An underscore inside a word is not emphasis, so intra_word_underscores
    // stay literal.
    if (character == '_') {
      can_open = left_flanking && (!right_flanking || before_punctuation);
      can_close = right_flanking && (!left_flanking || after_punctuation);
    }

    flush_text();

    Node* node = make(NodeKind::text);
    node->literal = arena_.intern(input_.substr(start, position_ - start));
    nodes_.push_back(node);

    delimiters_.push_back(Delimiter{node, character, count, count, can_open, can_close, true, nodes_.size() - 1});
  }

  // The specification's emphasis resolution: walk forward to each closer, then
  // back to the nearest matching opener, wrapping what lies between.
  void process_emphasis(std::size_t floor) {
    if (delimiters_.size() <= floor) {
      return;
    }

    std::size_t closer_index = floor;

    while (closer_index < delimiters_.size()) {
      Delimiter& closer = delimiters_[closer_index];

      if (!closer.active || !closer.can_close || closer.count == 0) {
        ++closer_index;
        continue;
      }

      bool found = false;
      std::size_t opener_index = closer_index;

      while (opener_index > floor) {
        --opener_index;

        Delimiter& opener = delimiters_[opener_index];

        if (!opener.active || !opener.can_open || opener.character != closer.character || opener.count == 0) {
          continue;
        }

        // A run that can both open and close only matches when the combined
        // length is not a multiple of three, or both are.
        const bool tricky = closer.character != '~' && (closer.can_open || opener.can_close) &&
                            (closer.original_count + opener.original_count) % 3 == 0 &&
                            (closer.original_count % 3 != 0 || opener.original_count % 3 != 0);

        if (tricky) {
          continue;
        }

        found = true;
        break;
      }

      if (!found) {
        if (!closer.can_open) {
          closer.active = false;
        }

        ++closer_index;
        continue;
      }

      Delimiter& opener = delimiters_[opener_index];

      const int used = (opener.count >= 2 && closer.count >= 2) ? 2 : 1;

      Node* wrapper = make(closer.character == '~' ? NodeKind::strikethrough
                                                  : (used == 2 ? NodeKind::strong : NodeKind::emphasis));

      for (std::size_t index = opener.index + 1; index < closer.index; ++index) {
        if (nodes_[index] != nullptr) {
          append_child(wrapper, nodes_[index]);
          nodes_[index] = nullptr;
        }
      }

      opener.count -= used;
      closer.count -= used;

      trim_delimiter_node(opener);
      trim_delimiter_node(closer);

      // The wrapper occupies a slot inside the pair instead of being inserted,
      // so every index already recorded stays valid and an enclosing pair sees
      // this wrapper as one of its own children.
      if (opener.index + 1 < closer.index) {
        nodes_[opener.index + 1] = wrapper;
      } else if (nodes_[closer.index] == nullptr) {
        nodes_[closer.index] = wrapper;
      } else {
        nodes_[opener.index] = wrapper;
      }

      for (std::size_t between = opener_index + 1; between < closer_index; ++between) {
        delimiters_[between].active = false;
      }

      if (closer.count == 0) {
        closer.active = false;
      }

      if (opener.count == 0) {
        opener.active = false;
      }
    }

    compact();
  }

  void trim_delimiter_node(Delimiter& delimiter) {
    if (delimiter.count > 0) {
      delimiter.node->literal = arena_.intern(std::string(static_cast<std::size_t>(delimiter.count),
                                                          delimiter.character));

      return;
    }

    nodes_[delimiter.index] = nullptr;
  }

  void compact() {
    std::vector<Node*> rebuilt;
    rebuilt.reserve(nodes_.size());

    for (Node* node : nodes_) {
      if (node != nullptr) {
        rebuilt.push_back(node);
      }
    }

    // Recorded indices describe the layout with its holes, so both stacks are
    // retired once the holes are closed up.
    delimiters_.clear();
    brackets_.clear();

    nodes_ = std::move(rebuilt);
  }

  Arena& arena_;
  std::string_view input_;
  const ReferenceMap& references_;

  Node* parent_ = nullptr;
  std::string pending_;
  std::size_t position_ = 0;
  std::vector<Node*> nodes_;
  std::vector<Delimiter> delimiters_;
  std::vector<Bracket> brackets_;
};

}  // namespace

std::size_t match_html_tag(std::string_view text) {
  return text.size() < 2 || text[0] != '<' ? 0 : match_tag_impl(text);
}

std::size_t parse_reference_definition(Arena& arena, std::string_view input, ReferenceMap& references) {
  std::size_t position = 0;

  while (position < input.size() && is_space_or_tab(input[position])) {
    ++position;
  }

  if (position >= input.size() || input[position] != '[') {
    return 0;
  }

  const std::size_t label_start = ++position;
  int depth = 0;

  while (position < input.size()) {
    if (input[position] == '\\' && position + 1 < input.size()) {
      position += 2;
      continue;
    }

    if (input[position] == '[') {
      ++depth;
    } else if (input[position] == ']') {
      if (depth == 0) {
        break;
      }

      --depth;
    }

    ++position;
  }

  if (position >= input.size() || input[position] != ']') {
    return 0;
  }

  const std::string label(input.substr(label_start, position - label_start));
  ++position;

  if (position >= input.size() || input[position] != ':' || normalize_label(label).empty()) {
    return 0;
  }

  ++position;

  while (position < input.size() && is_whitespace(input[position])) {
    ++position;
  }

  std::string destination;
  bool angled = false;

  if (position < input.size() && input[position] == '<') {
    std::size_t probe = position + 1;
    std::string bracketed;
    bool closed = false;

    while (probe < input.size()) {
      if (input[probe] == '\\' && probe + 1 < input.size() && is_punctuation(input[probe + 1])) {
        bracketed += input[probe + 1];
        probe += 2;
        continue;
      }

      if (input[probe] == '\n' || input[probe] == '<') {
        break;
      }

      if (input[probe] == '>') {
        closed = true;
        break;
      }

      bracketed += input[probe];
      ++probe;
    }

    if (!closed) {
      return 0;
    }

    angled = true;
    destination = bracketed;
    position = probe + 1;
  } else {
    while (position < input.size() && !is_whitespace(input[position])) {
      if (input[position] == '\\' && position + 1 < input.size() && is_punctuation(input[position + 1])) {
        destination += input[position + 1];
        position += 2;
        continue;
      }

      destination += input[position];
      ++position;
    }
  }

  if (destination.empty() && !angled) {
    return 0;
  }

  const std::size_t after_destination = position;

  // A title has to be separated from the destination. Without this
  // "[foo]: <bar>(baz)" would read as a definition, not as text.
  std::size_t separators = 0;

  while (position < input.size() && is_space_or_tab(input[position])) {
    ++position;
    ++separators;
  }

  bool crossed_newline = false;

  if (position < input.size() && input[position] == '\n') {
    crossed_newline = true;
    ++position;

    while (position < input.size() && is_space_or_tab(input[position])) {
      ++position;
    }
  }

  std::string title;
  std::size_t after_title = after_destination;

  if (separators + (crossed_newline ? 1 : 0) > 0 && position < input.size() &&
      (input[position] == '"' || input[position] == '\'' || input[position] == '(')) {
    const char opener = input[position];
    const char closer = opener == '(' ? ')' : opener;
    std::size_t scan = position + 1;
    bool closed = false;

    while (scan < input.size()) {
      if (input[scan] == '\\' && scan + 1 < input.size() && is_punctuation(input[scan + 1])) {
        title += input[scan + 1];
        scan += 2;
        continue;
      }

      if (input[scan] == closer) {
        closed = true;
        break;
      }

      title += input[scan];
      ++scan;
    }

    if (closed) {
      std::size_t tail = scan + 1;

      while (tail < input.size() && is_space_or_tab(input[tail])) {
        ++tail;
      }

      if (tail >= input.size() || input[tail] == '\n') {
        after_title = tail < input.size() ? tail + 1 : tail;
      } else {
        title.clear();
      }
    } else {
      title.clear();
    }
  }

  if (after_title == after_destination) {
    // No title, so the definition ends at the end of its own line.
    std::size_t tail = after_destination;

    while (tail < input.size() && is_space_or_tab(input[tail])) {
      ++tail;
    }

    if (tail < input.size() && input[tail] != '\n') {
      return 0;
    }

    after_title = tail < input.size() ? tail + 1 : tail;
    title.clear();
  } else if (crossed_newline && title.empty()) {
    return 0;
  }

  const std::string key = normalize_label(unescape_string(label));

  if (!references.contains(key)) {
    references.emplace(key, Reference{arena.intern(unescape_string(destination)),
                                      arena.intern(unescape_string(title))});
  }

  return after_title;
}

std::size_t parse_footnote_definition(Arena& arena, std::string_view input, FootnoteMap& footnotes) {
  std::size_t position = 0;

  while (position < input.size() && is_space_or_tab(input[position])) {
    ++position;
  }

  if (position + 2 >= input.size() || input[position] != '[' || input[position + 1] != '^') {
    return 0;
  }

  const std::size_t label_start = position + 2;
  position = label_start;

  while (position < input.size() && (is_letter(input[position]) || is_digit(input[position]) ||
                                     input[position] == '-' || input[position] == '_')) {
    ++position;
  }

  if (position == label_start || position + 1 >= input.size() || input[position] != ']' ||
      input[position + 1] != ':') {
    return 0;
  }

  const std::string label(input.substr(label_start, position - label_start));
  position += 2;

  while (position < input.size() && is_space_or_tab(input[position])) {
    ++position;
  }

  const std::size_t body_start = position;
  const auto newline = input.find('\n', position);
  const std::size_t body_end = newline == std::string_view::npos ? input.size() : newline;

  if (!footnotes.contains(label)) {
    footnotes.emplace(label, arena.intern(input.substr(body_start, body_end - body_start)));
  }

  return newline == std::string_view::npos ? input.size() : newline + 1;
}

namespace {

void collect_footnote_refs(Node* node, std::vector<Node*>& refs) {
  for (Node* child = node->first_child; child != nullptr; child = child->next_sibling) {
    if (child->kind == NodeKind::footnote_ref) {
      refs.push_back(child);
    }

    collect_footnote_refs(child, refs);
  }
}

}  // namespace

Node* resolve_footnotes(Arena& arena, Node* document, const FootnoteMap& footnotes,
                        const ReferenceMap& references) {
  if (footnotes.empty()) {
    return nullptr;
  }

  std::vector<Node*> refs;
  collect_footnote_refs(document, refs);

  std::unordered_map<std::string, int> numbers;
  std::unordered_map<std::string, int> occurrences;
  std::vector<std::string> order;

  for (Node* ref : refs) {
    const std::string label(ref->label);

    if (!footnotes.contains(label)) {
      continue;
    }

    if (!numbers.contains(label)) {
      numbers.emplace(label, static_cast<int>(order.size()) + 1);
      order.push_back(label);
    }

    ref->number = numbers[label];
    ref->occurrence = ++occurrences[label];
  }

  if (order.empty()) {
    return nullptr;
  }

  Node* section = arena.create<Node>();
  section->kind = NodeKind::footnotes;

  for (const std::string& label : order) {
    Node* item = arena.create<Node>();
    item->kind = NodeKind::footnote_item;
    item->label = arena.intern(label);
    item->number = numbers[label];
    item->literal = footnotes.at(label);

    append_child(section, item);

    InlineParser parser(arena, item->literal, references);
    parser.parse_into(item);
    item->literal = {};
  }

  return section;
}

void parse_inlines(Arena& arena, Node* root, const ReferenceMap& references) {
  if (root == nullptr) {
    return;
  }

  if (root->kind == NodeKind::paragraph || root->kind == NodeKind::heading ||
      root->kind == NodeKind::table_cell || root->kind == NodeKind::definition_term ||
      root->kind == NodeKind::definition_detail) {
    InlineParser parser(arena, root->literal, references);
    parser.parse_into(root);
    root->literal = {};

    return;
  }

  for (Node* child = root->first_child; child != nullptr; child = child->next_sibling) {
    parse_inlines(arena, child, references);
  }
}

}  // namespace blogin::markdown_detail
