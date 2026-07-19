#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "arena.h"
#include "markdown.h"

namespace blogin {

enum class TemplateOp {
  root,
  element,
  literal,
  output_escaped,
  output_raw,
  yield_body,
};

struct TemplateNode {
  TemplateOp op = TemplateOp::root;
  std::string_view tag;
  std::string_view text;
  TemplateNode* first_child = nullptr;
  TemplateNode* last_child = nullptr;
  TemplateNode* next_sibling = nullptr;
};

// Values a template can read while rendering. Immutable for the life of a
// render, so many threads share one without synchronizing.
class Context {
public:
  void set(std::string name, std::string value) { values_.emplace(std::move(name), std::move(value)); }

  void set_body(std::string body) { body_ = std::move(body); }

  std::string_view lookup(std::string_view name) const;

  std::string_view body() const { return body_; }

private:
  std::unordered_map<std::string, std::string> values_;
  std::string body_;
};

// A template compiled once and rendered many times.
//
// Compilation happens on construction and nothing mutates afterward, so a
// single instance is rendered concurrently from any number of threads with no
// lock on the render path. It owns its source buffer, which the node views
// borrow from.
class CompiledTemplate {
public:
  static CompiledTemplate compile(std::string source);

  const TemplateNode* root() const { return root_; }

  CompiledTemplate(CompiledTemplate&&) noexcept = default;
  CompiledTemplate& operator=(CompiledTemplate&&) noexcept = default;

private:
  CompiledTemplate() = default;

  std::unique_ptr<Source> source_;
  std::unique_ptr<Arena> arena_;
  const TemplateNode* root_ = nullptr;
};

void render_template(std::string& out, const CompiledTemplate& compiled, const Context& context);

std::string render_template(const CompiledTemplate& compiled, const Context& context);

}  // namespace blogin
