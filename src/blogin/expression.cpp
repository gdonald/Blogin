#include "expression.h"

#include <charconv>
#include <format>
#include <utility>

#include "text.h"

namespace blogin::expression {
namespace {

bool is_name_start(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         character == '_';
}

bool is_name_character(char character) {
  return is_name_start(character) || (character >= '0' && character <= '9') || character == '-';
}

bool is_digit(char character) {
  return character >= '0' && character <= '9';
}

class Parser {
public:
  // Far past anything a layout writes, and far short of what the stack holds.
  static constexpr int max_depth = 64;

  Parser(std::string_view source, std::size_t line, std::size_t column)
    : source_(source), line_(line), base_column_(column) {}

  std::expected<std::unique_ptr<Node>, ParseError> parse() {
    auto result = parse_or();

    if (!result) {
      return result;
    }

    skip_spaces();

    if (position_ < source_.size()) {
      // Assignment is refused by name rather than as a stray character, since
      // that is what somebody who wrote it was trying to do.
      if (source_[position_] == '=') {
        return fail("assignment is not supported");
      }

      return fail(std::format("unexpected '{}'", source_.substr(position_, 1)));
    }

    return result;
  }

private:
  std::unexpected<ParseError> fail(std::string message) const {
    return std::unexpected(ParseError{std::move(message), line_, base_column_ + position_});
  }

  std::unique_ptr<Node> make(NodeKind kind) const {
    auto node = std::make_unique<Node>();
    node->kind = kind;
    node->line = line_;
    node->column = base_column_ + position_;

    return node;
  }

  void skip_spaces() {
    while (position_ < source_.size() && (source_[position_] == ' ' || source_[position_] == '\t')) {
      ++position_;
    }
  }

  bool done() {
    skip_spaces();

    return position_ >= source_.size();
  }

  char peek() const { return position_ < source_.size() ? source_[position_] : '\0'; }

  bool consume(std::string_view token) {
    skip_spaces();

    if (source_.compare(position_, token.size(), token) != 0) {
      return false;
    }

    // A word operator has to be a whole word, or "note" would read as "not e".
    if (is_name_start(token.front())) {
      const std::size_t after = position_ + token.size();

      if (after < source_.size() && is_name_character(source_[after])) {
        return false;
      }
    }

    position_ += token.size();

    return true;
  }

  std::string read_name() {
    const std::size_t start = position_;

    while (position_ < source_.size() && is_name_character(source_[position_])) {
      ++position_;
    }

    return std::string(source_.substr(start, position_ - start));
  }

  // Every way back to the top of the grammar comes through here: a
  // parenthesised group, a call argument, a map value, a block. Counting the
  // trips down is what stops input like 20,000 open parens from recursing until
  // the stack runs out, which is a crash rather than an error message.
  //
  // The evaluator walks the tree the same way, so a tree the parser refuses to
  // build is one the evaluator can never be handed.
  class Depth {
  public:
    explicit Depth(Parser& parser) : parser_(parser) { ++parser_.depth_; }

    ~Depth() { --parser_.depth_; }

    Depth(const Depth&) = delete;
    Depth& operator=(const Depth&) = delete;

    bool too_deep() const { return parser_.depth_ > max_depth; }

  private:
    Parser& parser_;
  };

  std::expected<std::unique_ptr<Node>, ParseError> parse_or() {
    const Depth depth(*this);

    if (depth.too_deep()) {
      return fail(std::format("nested too deeply, past {} levels", max_depth));
    }

    auto left = parse_and();

    if (!left) {
      return left;
    }

    while (consume("||") || consume("or")) {
      auto right = parse_and();

      if (!right) {
        return right;
      }

      auto node = make(NodeKind::binary);
      node->op = BinaryOperator::logical_or;
      node->left = std::move(*left);
      node->right = std::move(*right);
      left = std::move(node);
    }

    return left;
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_and() {
    auto left = parse_comparison();

    if (!left) {
      return left;
    }

    while (consume("&&") || consume("and")) {
      auto right = parse_comparison();

      if (!right) {
        return right;
      }

      auto node = make(NodeKind::binary);
      node->op = BinaryOperator::logical_and;
      node->left = std::move(*left);
      node->right = std::move(*right);
      left = std::move(node);
    }

    return left;
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_comparison() {
    auto left = parse_additive();

    if (!left) {
      return left;
    }

    while (true) {
      BinaryOperator op{};

      // "<=" and "<" are checked before the subscript form, which only appears
      // after a postfix target.
      if (consume("==") || consume("eq")) {
        op = BinaryOperator::equal;
      } else if (consume("!=") || consume("ne")) {
        op = BinaryOperator::not_equal;
      } else if (consume("<=")) {
        op = BinaryOperator::less_or_equal;
      } else if (consume(">=")) {
        op = BinaryOperator::greater_or_equal;
      } else if (consume("<")) {
        op = BinaryOperator::less;
      } else if (consume(">")) {
        op = BinaryOperator::greater;
      } else {
        break;
      }

      auto right = parse_additive();

      if (!right) {
        return right;
      }

      auto node = make(NodeKind::binary);
      node->op = op;
      node->left = std::move(*left);
      node->right = std::move(*right);
      left = std::move(node);
    }

    return left;
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_additive() {
    auto left = parse_unary();

    if (!left) {
      return left;
    }

    while (true) {
      BinaryOperator op{};

      if (consume("~")) {
        op = BinaryOperator::concatenate;
      } else if (consume("+")) {
        op = BinaryOperator::add;
      } else if (consume("-")) {
        op = BinaryOperator::subtract;
      } else {
        break;
      }

      auto right = parse_unary();

      if (!right) {
        return right;
      }

      auto node = make(NodeKind::binary);
      node->op = op;
      node->left = std::move(*left);
      node->right = std::move(*right);
      left = std::move(node);
    }

    return left;
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_unary() {
    // A prefix operator recurses into this function directly rather than back
    // through the top of the grammar, so the counter parse_or keeps does not
    // move for a run of them and this needs its own guard.
    const Depth depth(*this);

    if (depth.too_deep()) {
      return fail(std::format("nested too deeply, past {} levels", max_depth));
    }

    if (consume("!") || consume("not")) {
      auto operand = parse_unary();

      if (!operand) {
        return operand;
      }

      auto node = make(NodeKind::unary_not);
      node->target = std::move(*operand);

      return node;
    }

    return parse_postfix();
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_postfix() {
    // The analyzer does not model the destructor of the expected holding this
    // pointer, so it reports every parse that returns one as a leak.
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    auto target = parse_primary();

    if (!target) {
      return target;
    }

    while (true) {
      if (consume(".")) {
        const std::string member = read_name();

        if (member.empty()) {
          return fail("expected a name after '.'");
        }

        auto node = make(NodeKind::member);
        node->text = member;
        node->target = std::move(*target);

        // A member may itself be called: $node.children.elems, url().
        if (peek() == '(') {
          auto arguments = parse_arguments();

          if (!arguments) {
            return std::unexpected(arguments.error());
          }

          auto call = make(NodeKind::call);
          call->target = std::move(node);
          call->arguments = std::move(*arguments);
          target = std::move(call);

          continue;
        }

        target = std::move(node);
        continue;
      }

      // The angle-bracket subscript, `$tag<url>`, which reads a map key the
      // same way `.url` does.
      if (peek() == '<') {
        const std::size_t saved = position_;
        ++position_;

        const std::string key = read_name();

        if (!key.empty() && peek() == '>') {
          ++position_;

          auto node = make(NodeKind::member);
          node->text = key;
          node->target = std::move(*target);
          target = std::move(node);

          continue;
        }

        position_ = saved;
        break;
      }

      if (peek() == '(') {
        auto arguments = parse_arguments();

        if (!arguments) {
          return std::unexpected(arguments.error());
        }

        auto call = make(NodeKind::call);
        call->target = std::move(*target);
        call->arguments = std::move(*arguments);
        target = std::move(call);

        continue;
      }

      break;
    }

    return target;
  }

  std::expected<std::vector<std::unique_ptr<Node>>, ParseError> parse_arguments() {
    std::vector<std::unique_ptr<Node>> arguments;

    ++position_;

    if (done()) {
      return std::unexpected(fail("unterminated argument list").error());
    }

    if (peek() == ')') {
      ++position_;

      return arguments;
    }

    while (true) {
      auto argument = parse_argument();

      if (!argument) {
        return std::unexpected(argument.error());
      }

      arguments.push_back(std::move(*argument));

      skip_spaces();

      if (consume(",")) {
        continue;
      }

      if (consume(")")) {
        break;
      }

      return std::unexpected(fail("expected ',' or ')' in an argument list").error());
    }

    return arguments;
  }

  // :name<text> and :name(expression), the way the layouts write them.
  std::expected<std::unique_ptr<Node>, ParseError> parse_argument() {
    skip_spaces();

    if (peek() != ':') {
      return parse_or();
    }

    ++position_;

    const std::string name = read_name();

    if (name.empty()) {
      return fail("expected a name after ':'");
    }

    auto node = make(NodeKind::named_argument);
    node->text = name;

    if (peek() == '<') {
      const std::size_t closing = source_.find('>', position_);

      if (closing == std::string_view::npos) {
        return fail("unterminated named argument");
      }

      auto literal = make(NodeKind::literal_string);
      literal->text = std::string(source_.substr(position_ + 1, closing - position_ - 1));

      node->target = std::move(literal);
      position_ = closing + 1;

      return node;
    }

    if (peek() == '(') {
      ++position_;

      auto value = parse_or();

      if (!value) {
        return value;
      }

      if (!consume(")")) {
        return fail("expected ')' after a named argument");
      }

      node->target = std::move(*value);

      return node;
    }

    // A bare :name is the name itself, which is how :as<entry> degenerates.
    auto literal = make(NodeKind::literal_string);
    literal->text = name;
    node->target = std::move(literal);

    return node;
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_primary() {
    skip_spaces();

    if (position_ >= source_.size()) {
      return fail("expected a value");
    }

    const char character = source_[position_];

    if (character == '(') {
      ++position_;

      auto inner = parse_or();

      if (!inner) {
        return inner;
      }

      if (!consume(")")) {
        return fail("expected ')'");
      }

      return inner;
    }

    if (character == '{') {
      return looks_like_map() ? parse_map() : parse_block();
    }

    if (character == '"' || character == '\'') {
      return parse_string(character);
    }

    if (is_digit(character)) {
      return parse_number();
    }

    if (character == '$') {
      ++position_;

      const std::string name = read_name();

      if (name.empty()) {
        return fail("expected a name after '$'");
      }

      auto node = make(NodeKind::variable);
      node->text = name;

      return node;
    }

    if (is_name_start(character)) {
      const std::size_t start = position_;
      const std::string word = read_name();

      // Capitalized spellings are accepted too, since layouts use both.
      if (word == "true" || word == "false" || word == "True" || word == "False") {
        auto node = make(NodeKind::literal_boolean);
        node->boolean = word == "true" || word == "True";

        return node;
      }

      if (word == "null" || word == "Nil") {
        return make(NodeKind::literal_null);
      }

      auto node = make(NodeKind::name);
      node->text = word;
      node->column = base_column_ + start;

      return node;
    }

    if (character == '=') {
      return fail("assignment is not supported");
    }

    return fail(std::format("unexpected '{}'", source_.substr(position_, 1)));
  }

  // Both a map and a deferred block open with '{'. A map's first thing is a name
  // followed by a colon, and nothing else can start that way, so one character
  // of lookahead past the name settles it.
  bool looks_like_map() const {
    std::size_t scan = position_ + 1;

    while (scan < source_.size() && (source_[scan] == ' ' || source_[scan] == '\t')) {
      ++scan;
    }

    // An empty map, written the obvious way.
    if (scan < source_.size() && source_[scan] == '}') {
      return true;
    }

    if (scan >= source_.size() || !is_name_start(source_[scan])) {
      return false;
    }

    while (scan < source_.size() && is_name_character(source_[scan])) {
      ++scan;
    }

    while (scan < source_.size() && (source_[scan] == ' ' || source_[scan] == '\t')) {
      ++scan;
    }

    return scan < source_.size() && source_[scan] == ':';
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_map() {
    auto node = make(NodeKind::map);

    ++position_;
    skip_spaces();

    while (position_ < source_.size() && source_[position_] != '}') {
      const std::string key = read_name();

      if (key.empty()) {
        return fail("expected a name as a map key");
      }

      skip_spaces();

      if (!consume(":")) {
        return fail(std::format("expected ':' after the map key '{}'", key));
      }

      auto value = parse_or();

      if (!value) {
        return value;
      }

      auto entry = make(NodeKind::named_argument);
      entry->text = key;
      entry->target = std::move(*value);

      node->arguments.push_back(std::move(entry));

      skip_spaces();

      if (!consume(",")) {
        break;
      }

      skip_spaces();
    }

    if (!consume("}")) {
      return fail("expected '}' to close a map");
    }

    return node;
  }

  // The one deferred form. Not a closure: it takes no parameters and cannot be
  // stored or passed anywhere else.
  std::expected<std::unique_ptr<Node>, ParseError> parse_block() {
    ++position_;

    auto inner = parse_or();

    if (!inner) {
      return inner;
    }

    if (!consume("}")) {
      return fail("expected '}' to close a block");
    }

    auto node = make(NodeKind::block);
    node->target = std::move(*inner);

    return node;
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_string(char quote) {
    ++position_;

    std::string value;

    while (position_ < source_.size() && source_[position_] != quote) {
      if (source_[position_] == '\\' && position_ + 1 < source_.size()) {
        ++position_;
      }

      value += source_[position_++];
    }

    if (position_ >= source_.size()) {
      return fail("unterminated string");
    }

    ++position_;

    auto node = make(NodeKind::literal_string);
    node->text = std::move(value);

    return node;
  }

  std::expected<std::unique_ptr<Node>, ParseError> parse_number() {
    const std::size_t start = position_;

    while (position_ < source_.size() && is_digit(source_[position_])) {
      ++position_;
    }

    bool floating = false;

    if (position_ < source_.size() && source_[position_] == '.' && position_ + 1 < source_.size() &&
        is_digit(source_[position_ + 1])) {
      floating = true;
      ++position_;

      while (position_ < source_.size() && is_digit(source_[position_])) {
        ++position_;
      }
    }

    const std::string_view token = source_.substr(start, position_ - start);

    if (floating) {
      auto node = make(NodeKind::literal_number);
      node->number = text::to_double(token).value_or(0.0);

      return node;
    }

    std::int64_t value = 0;
    std::from_chars(token.data(), token.data() + token.size(), value);

    auto node = make(NodeKind::literal_integer);
    node->integer = value;

    return node;
  }

  int depth_ = 0;
  std::string_view source_;
  std::size_t position_ = 0;
  std::size_t line_ = 1;
  std::size_t base_column_ = 1;
};

}  // namespace

std::expected<std::unique_ptr<Node>, ParseError> parse(std::string_view source, std::size_t line,
                                                        std::size_t column) {
  Parser parser(text::trim(source), line, column);

  return parser.parse();
}

}  // namespace blogin::expression

namespace blogin::expression {
namespace {

std::unexpected<ParseError> error_at(const Node& node, std::string message) {
  return std::unexpected(ParseError{std::move(message), node.line, node.column});
}

bool same_value(const Value& left, const Value& right) {
  // Comparing a number against a string is almost always a template mistake
  // rather than an intent, but returning false is friendlier than refusing.
  if (left.is_string() || right.is_string()) {
    return left.is_string() && right.is_string() && left.as_string() == right.as_string();
  }

  if (left.is_null() || right.is_null()) {
    return left.is_null() && right.is_null();
  }

  if (left.is_boolean() || right.is_boolean()) {
    return left.is_boolean() && right.is_boolean() && left.as_boolean() == right.as_boolean();
  }

  return left.as_number() == right.as_number();
}

std::expected<int, ParseError> compare(const Node& node, const Value& left, const Value& right) {
  if (left.is_string() && right.is_string()) {
    const std::string_view a = left.as_string();
    const std::string_view b = right.as_string();

    return a < b ? -1 : (a == b ? 0 : 1);
  }

  if ((left.is_integer() || left.is_number()) && (right.is_integer() || right.is_number())) {
    const double a = left.as_number();
    const double b = right.as_number();

    return a < b ? -1 : (a == b ? 0 : 1);
  }

  return std::unexpected(
    error_at(node, std::format("cannot compare {} with {}", left.type_name(), right.type_name())).error());
}

std::string to_text(const Value& value) {
  switch (value.type()) {
    case Value::Type::null:
      return {};
    case Value::Type::boolean:
      return value.as_boolean() ? "true" : "false";
    case Value::Type::integer:
      return std::format("{}", value.as_integer());
    case Value::Type::number:
      return std::format("{}", value.as_number());
    case Value::Type::string:
      return std::string(value.as_string());
    default:
      return {};
  }
}

// A list answers a few questions of its own, so a layout can ask whether it has
// anything in it without the view offering a helper for every collection.
std::expected<Value, ParseError> list_member(const Node& node, const Value& target, std::string_view name) {
  if (name == "elems" || name == "size" || name == "count") {
    return Value(static_cast<std::int64_t>(target.size()));
  }

  if (name == "first") {
    return target.at(0);
  }

  if (name == "last") {
    return target.empty() ? Value() : target.at(target.size() - 1);
  }

  return std::unexpected(error_at(node, std::format("a list has no '{}'", name)).error());
}

}  // namespace

std::expected<Value, ParseError> evaluate(const Node& node, ViewContext& context) {
  switch (node.kind) {
    case NodeKind::literal_null:
      return Value();

    case NodeKind::literal_boolean:
      return Value(node.boolean);

    case NodeKind::literal_integer:
      return Value(node.integer);

    case NodeKind::literal_number:
      return Value(node.number);

    case NodeKind::literal_string:
      return Value(node.text);

    case NodeKind::variable: {
      if (const Value* local = context.lookup_local(node.text)) {
        return *local;
      }

      return error_at(node, std::format("no such local '${}'", node.text));
    }

    case NodeKind::name: {
      // A local shadows a view name, which is how a loop variable works.
      if (const Value* local = context.lookup_local(node.text)) {
        return *local;
      }

      if (const Value* value = context.lookup(node.text)) {
        return *value;
      }

      // A name with no arguments may still be something to call: url and url()
      // mean the same thing.
      if (const ViewContext::Function* function = context.function(node.text)) {
        return (*function)({});
      }

      return error_at(node, std::format("no such name '{}'{}", node.text, context.nearest(node.text)));
    }

    case NodeKind::member: {
      auto target = evaluate(*node.target, context);

      if (!target) {
        return target;
      }

      if (target->is_array()) {
        return list_member(node, *target, node.text);
      }

      if (target->is_object()) {
        // A missing key reads as null, because asking whether something is
        // there is how a layout decides whether to render it.
        return (*target)[node.text];
      }

      if (target->is_string() && (node.text == "chars" || node.text == "elems")) {
        return Value(static_cast<std::int64_t>(target->size()));
      }

      return error_at(node, std::format("cannot read '{}' from {}", node.text, target->type_name()));
    }

    case NodeKind::named_argument:
      return evaluate(*node.target, context);

    case NodeKind::block:
      // A block is only meaningful to a function that defers it, and reaching
      // here means one was written where a value belongs.
      return error_at(node, "a block is only allowed as an argument");

    case NodeKind::map: {
      Value out = Value::object();

      for (const auto& entry : node.arguments) {
        auto value = evaluate(*entry->target, context);

        if (!value) {
          return value;
        }

        out.set(entry->text, std::move(*value));
      }

      return out;
    }

    case NodeKind::unary_not: {
      auto operand = evaluate(*node.target, context);

      if (!operand) {
        return operand;
      }

      return Value(!operand->truthy());
    }

    case NodeKind::binary: {
      auto left = evaluate(*node.left, context);

      if (!left) {
        return left;
      }

      // Both of these stop early, so "- if post && post<title>" is safe.
      if (node.op == BinaryOperator::logical_and) {
        if (!left->truthy()) {
          return *left;
        }

        return evaluate(*node.right, context);
      }

      if (node.op == BinaryOperator::logical_or) {
        if (left->truthy()) {
          return *left;
        }

        return evaluate(*node.right, context);
      }

      auto right = evaluate(*node.right, context);

      if (!right) {
        return right;
      }

      switch (node.op) {
        case BinaryOperator::equal:
          return Value(same_value(*left, *right));

        case BinaryOperator::not_equal:
          return Value(!same_value(*left, *right));

        case BinaryOperator::concatenate:
          return Value(to_text(*left) + to_text(*right));

        case BinaryOperator::add:
          if (left->is_string() || right->is_string()) {
            return Value(to_text(*left) + to_text(*right));
          }

          if (left->is_integer() && right->is_integer()) {
            return Value(left->as_integer() + right->as_integer());
          }

          return Value(left->as_number() + right->as_number());

        case BinaryOperator::subtract:
          if (left->is_integer() && right->is_integer()) {
            return Value(left->as_integer() - right->as_integer());
          }

          return Value(left->as_number() - right->as_number());

        default:
          break;
      }

      auto ordering = compare(node, *left, *right);

      if (!ordering) {
        return std::unexpected(ordering.error());
      }

      switch (node.op) {
        case BinaryOperator::less:
          return Value(*ordering < 0);
        case BinaryOperator::less_or_equal:
          return Value(*ordering <= 0);
        case BinaryOperator::greater:
          return Value(*ordering > 0);
        case BinaryOperator::greater_or_equal:
          return Value(*ordering >= 0);
        default:
          return error_at(node, "unsupported operator");
      }
    }

    case NodeKind::call: {
      if (node.target->kind != NodeKind::name && node.target->kind != NodeKind::member) {
        return error_at(node, "only a name can be called");
      }

      const std::string& name = node.target->text;

      // A member call on a value, such as $node.children.elems, is a read
      // rather than a call into the view.
      if (node.target->kind == NodeKind::member && node.arguments.empty()) {
        return evaluate(*node.target, context);
      }

      const ViewContext::Function* function = context.function(name);

      if (function == nullptr) {
        // `url` and `url()` mean the same thing, so calling a plain value with
        // no arguments reads it.
        if (node.arguments.empty()) {
          if (const Value* local = context.lookup_local(name)) {
            return *local;
          }

          if (const Value* value = context.lookup(name)) {
            return *value;
          }
        }

        return error_at(node, std::format("no such name '{}'{}", name, context.nearest(name)));
      }

      std::vector<ViewContext::Argument> arguments;

      for (const auto& argument : node.arguments) {
        // A deferred block reaches the function unevaluated, which is the whole
        // point of the form.
        if (argument->kind == NodeKind::block) {
          auto value = evaluate(*argument->target, context);

          if (!value) {
            return value;
          }

          arguments.push_back(ViewContext::Argument{{}, std::move(*value)});
          continue;
        }

        auto value = evaluate(*argument, context);

        if (!value) {
          return value;
        }

        arguments.push_back(ViewContext::Argument{
          argument->kind == NodeKind::named_argument ? argument->text : std::string{}, std::move(*value)});
      }

      return (*function)(arguments);
    }
  }

  return error_at(node, "unsupported expression");
}

}  // namespace blogin::expression
