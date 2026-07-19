#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <functional>

#include "view_context.h"
#include "json.h"

namespace blogin::expression {

enum class NodeKind {
  literal_null,
  literal_boolean,
  literal_integer,
  literal_number,
  literal_string,

  // A bare name, asked of the view.
  name,

  // $local
  variable,

  // .member or <key>
  member,

  call,

  // :name<text> or :name(expression), only ever an argument
  named_argument,

  // { ... }, an expression whose evaluation is deferred
  block,

  // { name: expression, ... }, the one way to write a map
  map,

  unary_not,
  binary,
};

enum class BinaryOperator {
  equal,
  not_equal,
  less,
  less_or_equal,
  greater,
  greater_or_equal,
  logical_and,
  logical_or,
  add,
  subtract,
  concatenate,
};

// Parsed once when a template compiles, then read-only for the life of the
// build, so any number of threads render from it at once.
struct Node {
  NodeKind kind = NodeKind::literal_null;

  bool boolean = false;
  std::int64_t integer = 0;
  double number = 0.0;
  std::string text;

  BinaryOperator op = BinaryOperator::equal;

  // The receiver of a member read or a call, the operands of a binary, the
  // arguments of a call.
  std::unique_ptr<Node> target;
  std::unique_ptr<Node> left;
  std::unique_ptr<Node> right;
  std::vector<std::unique_ptr<Node>> arguments;

  std::size_t line = 1;
  std::size_t column = 1;
};

// Parses one expression. `line` is the template line it came from, so an error
// points at the layout rather than at the fragment.
std::expected<std::unique_ptr<Node>, ParseError> parse(std::string_view source, std::size_t line = 1,
                                                       std::size_t column = 1);

// What a call to a deferred block should produce. `cache-fragment` is the only
// caller, and it needs to run the block at most once.
using BlockEvaluator = std::function<Value()>;

// Evaluates against a context. Errors name the line, the column, and what was
// refused, so a template mistake is reported the way a compiler reports one.
std::expected<Value, ParseError> evaluate(const Node& node, ViewContext& context);

}  // namespace blogin::expression
