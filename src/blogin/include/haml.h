#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "expression.h"
#include "view_context.h"

namespace blogin::haml {

// A run of literal text with #{...} holes in it. Attribute values and plain
// text lines are both this shape.
struct Segment {
  std::string literal;
  std::unique_ptr<expression::Node> hole;
};

using Interpolated = std::vector<Segment>;

struct Attribute {
  std::string name;
  Interpolated value;

  // {disabled: true} writes a bare attribute rather than one with a value.
  bool boolean_shorthand = false;
};

enum class NodeKind {
  root,
  element,
  text,
  output,
  control,
  filter,
  doctype,
  comment,
};

enum class ControlKind {
  if_,
  elsif_,
  else_,
  unless_,
  for_,
};

struct Node {
  NodeKind kind = NodeKind::root;

  // element
  std::string tag;
  std::vector<std::string> classes;
  std::string id;
  std::vector<Attribute> attributes;
  bool self_closing = false;

  // text and output
  Interpolated text;
  std::unique_ptr<expression::Node> value;
  bool escaped = true;

  // control
  ControlKind control = ControlKind::if_;
  std::string loop_variable;

  // filter
  std::string filter;

  std::vector<std::unique_ptr<Node>> children;

  std::size_t line = 1;
};

// A template parsed once and rendered many times.
//
// Nothing mutates after compilation, so one instance is rendered concurrently
// from any number of threads with no lock on the render path.
class Template {
public:
  static std::expected<Template, ParseError> compile(std::string source, std::string name = {});

  const Node* root() const { return root_.get(); }

  std::string_view name() const { return name_; }

  Template(Template&&) noexcept = default;
  Template& operator=(Template&&) noexcept = default;

private:
  Template() = default;

  std::string source_;
  std::string name_;
  std::unique_ptr<Node> root_;
};

// Where a partial comes from. The engine renders it, and finding it is the
// caller's business, since where templates live belongs to a site rather than
// to the engine.
using PartialLookup = std::function<const Template*(std::string_view name)>;

// Fragments already rendered, keyed by what they read. Optional: without one,
// every fragment renders every time, which is correct but slower.
class FragmentStore {
public:
  virtual ~FragmentStore() = default;

  virtual const std::string* find(std::string_view key) const = 0;

  virtual void store(std::string key, std::string html) = 0;

  // What a fragment read the last time it rendered, so the next page can work
  // out its key by resolving those names rather than by rendering it. The site
  // is the compiled block, which is stable for the build: a template is
  // compiled once and never changes afterwards.
  virtual const std::vector<std::string>* reads(const void* site) const = 0;

  virtual void remember_reads(const void* site, std::vector<std::string> names) = 0;

  // A fragment body that had to be rendered. The count of these against the
  // number of pages is what says whether reuse is working.
  virtual void note_render() = 0;
};

struct RenderOptions {
  PartialLookup partial;

  FragmentStore* fragments = nullptr;

  // What `yield` writes.
  std::string body;
};

std::expected<std::string, ParseError> render(const Template& compiled, ViewContext& context,
                                              const RenderOptions& options = {});

}  // namespace blogin::haml
