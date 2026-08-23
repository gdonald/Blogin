#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <vector>
#include <utility>

#include "haml.h"
#include "text.h"

namespace blogin::haml {
namespace {

bool is_void_tag_name(std::string_view tag) {
  static const std::vector<std::string_view> tags{
    "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source",
    "track", "wbr",
  };

  return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

void escape(std::string& out, std::string_view value) {
  for (const char character : value) {
    switch (character) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += character; break;
    }
  }
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

class Renderer {
public:
  Renderer(ViewContext& context, const RenderOptions& options) : context_(context), options_(options) {}

  std::expected<std::string, ParseError> run(const Node* root) {
    if (auto result = render_children(*root); !result) {
      return std::unexpected(result.error());
    }

    return std::move(out_);
  }

private:
  std::expected<void, ParseError> render_children(const Node& node) {
    // A conditional chain is one decision, so the branches are walked together
    // instead of each being asked on its own.
    for (std::size_t index = 0; index < node.children.size();) {
      const Node& child = *node.children[index];

      if (child.kind == NodeKind::control && (child.control == ControlKind::if_ ||
                                              child.control == ControlKind::unless_)) {
        auto consumed = render_conditional(node, index);

        if (!consumed) {
          return std::unexpected(consumed.error());
        }

        index = *consumed;
        continue;
      }

      if (auto result = render(child); !result) {
        return result;
      }

      ++index;
    }

    return {};
  }

  std::expected<std::size_t, ParseError> render_conditional(const Node& parent, std::size_t index) {
    bool taken = false;
    bool first = true;

    while (index < parent.children.size()) {
      const Node& branch = *parent.children[index];

      if (branch.kind != NodeKind::control) {
        break;
      }

      const bool is_head = branch.control == ControlKind::if_ || branch.control == ControlKind::unless_;
      const bool is_tail = branch.control == ControlKind::elsif_ || branch.control == ControlKind::else_;

      // A second `if` starts its own chain. This has to be about position
      // within the chain, not position in the file: comparing against the
      // absolute index made the first branch of a later chain end the walk
      // without consuming anything, and the caller then spun on it forever.
      if (!first && is_head) {
        break;
      }

      if (!is_head && !is_tail) {
        break;
      }

      first = false;

      if (!taken) {
        bool condition = true;

        if (branch.control != ControlKind::else_) {
          auto value = expression::evaluate(*branch.value, context_);

          if (!value) {
            return std::unexpected(value.error());
          }

          condition = branch.control == ControlKind::unless_ ? !value->truthy() : value->truthy();
        }

        if (condition) {
          taken = true;

          if (auto result = render_children(branch); !result) {
            return std::unexpected(result.error());
          }
        }
      }

      ++index;

      if (index >= parent.children.size() || parent.children[index]->kind != NodeKind::control) {
        break;
      }

      const ControlKind next = parent.children[index]->control;

      if (next != ControlKind::elsif_ && next != ControlKind::else_) {
        break;
      }
    }

    return index;
  }

  std::expected<void, ParseError> render(const Node& node) {
    switch (node.kind) {
      case NodeKind::root:
        return render_children(node);

      case NodeKind::doctype:
        // Every doctype declaration writes the HTML5 one, whatever follows !!!.
        out_ += "<!DOCTYPE html>";
        out_ += '\n';
        return {};

      case NodeKind::comment:
        // A HAML comment is for whoever reads the template, not the page.
        return {};

      case NodeKind::text: {
        auto value = interpolate(node.text, true);

        if (!value) {
          return std::unexpected(value.error());
        }

        out_ += *value;
        out_ += '\n';
        return {};
      }

      case NodeKind::output:
        return render_output(node);

      case NodeKind::filter:
        return render_filter(node);

      case NodeKind::control:
        return render_loop(node);

      case NodeKind::element:
        break;
    }

    return render_element(node);
  }

  std::expected<void, ParseError> render_output(const Node& node) {
    if (auto handled = render_engine_call(*node.value); handled.has_value()) {
      if (!*handled) {
        return std::unexpected(handled->error());
      }

      out_ += node.escaped ? escaped_copy(**handled) : **handled;
      out_ += '\n';

      return {};
    }

    // `yield` is the layout's hole for the page it wraps.
    if (node.value->kind == expression::NodeKind::name && node.value->text == "yield") {
      out_ += options_.body;
      out_ += '\n';

      return {};
    }

    auto value = expression::evaluate(*node.value, context_);

    if (!value) {
      return std::unexpected(value.error());
    }

    const std::string rendered = to_text(*value);

    if (node.escaped) {
      escape(out_, rendered);
    } else {
      out_ += rendered;
    }

    out_ += '\n';
    return {};
  }

  std::expected<void, ParseError> render_loop(const Node& node) {
    if (node.control != ControlKind::for_) {
      // An elsif or else that reaches here has no if above it.
      return std::unexpected(
        ParseError{"'elsif' and 'else' need an 'if' above them", node.line, 1});
    }

    auto items = expression::evaluate(*node.value, context_);

    if (!items) {
      return std::unexpected(items.error());
    }

    if (!items->is_array()) {
      return std::unexpected(
        ParseError{std::format("cannot iterate {}", items->type_name()), node.line, 1});
    }

    auto saved = context_.take_locals();

    for (const Value& item : items->items()) {
      context_.restore_locals(saved);
      context_.set_local(node.loop_variable, item);

      if (auto result = render_children(node); !result) {
        context_.restore_locals(std::move(saved));

        return result;
      }
    }

    context_.restore_locals(std::move(saved));

    return {};
  }

  std::expected<void, ParseError> render_filter(const Node& node) {
    std::string body;

    for (const auto& child : node.children) {
      auto value = interpolate(child->text, false);

      if (!value) {
        return std::unexpected(value.error());
      }

      body += *value;
      body += '\n';
    }

    if (node.filter == "plain") {
      out_ += body;
      return {};
    }

    if (node.filter == "escaped") {
      escape(out_, body);
      return {};
    }

    if (node.filter == "javascript") {
      out_ += "<script>\n";
      out_ += body;
      out_ += "</script>\n";
      return {};
    }

    if (node.filter == "css") {
      out_ += "<style>\n";
      out_ += body;
      out_ += "</style>\n";
      return {};
    }

    return std::unexpected(ParseError{std::format("no such filter ':{}'", node.filter), node.line, 1});
  }

  std::expected<void, ParseError> render_element(const Node& node) {
    out_ += '<';
    out_ += node.tag;

    // Classes written as .one.two and in an attribute list combine.
    std::string classes;

    for (const std::string& name : node.classes) {
      classes += classes.empty() ? name : " " + name;
    }

    std::string id = node.id;

    for (const Attribute& attribute : node.attributes) {
      if (attribute.boolean_shorthand) {
        continue;
      }

      auto value = interpolate(attribute.value, false);

      if (!value) {
        return std::unexpected(value.error());
      }

      if (attribute.name == "class") {
        if (!value->empty()) {
          classes += classes.empty() ? *value : " " + *value;
        }

        continue;
      }

      if (attribute.name == "id") {
        id = *value;
        continue;
      }
    }

    if (!id.empty()) {
      out_ += " id=\"";
      escape(out_, id);
      out_ += '"';
    }

    if (!classes.empty()) {
      out_ += " class=\"";
      escape(out_, classes);
      out_ += '"';
    }

    for (const Attribute& attribute : node.attributes) {
      if (attribute.name == "class" || attribute.name == "id") {
        continue;
      }

      if (attribute.boolean_shorthand) {
        out_ += ' ';
        out_ += attribute.name;
        continue;
      }

      auto value = interpolate(attribute.value, false);

      if (!value) {
        return std::unexpected(value.error());
      }

      if (attribute.value.size() == 1 && attribute.value[0].hole != nullptr) {
        auto evaluated = expression::evaluate(*attribute.value[0].hole, context_);

        if (!evaluated) {
          return std::unexpected(evaluated.error());
        }

        if (evaluated->is_boolean()) {
          if (evaluated->as_boolean()) {
            out_ += ' ';
            out_ += attribute.name;
          }

          continue;
        }

        if (evaluated->is_null()) {
          continue;
        }
      }

      out_ += ' ';
      out_ += attribute.name;
      out_ += "=\"";
      escape(out_, *value);
      out_ += '"';
    }

    if (node.self_closing || is_void_tag_name(node.tag)) {
      out_ += " />\n";

      return {};
    }

    out_ += '>';

    if (node.children.empty()) {
      out_ += "</";
      out_ += node.tag;
      out_ += ">\n";

      return {};
    }

    // One inline child sits on the same line as its tag. Anything else gets its
    // own lines.
    const bool inline_child = node.children.size() == 1 &&
                              (node.children[0]->kind == NodeKind::text ||
                               node.children[0]->kind == NodeKind::output);

    if (!inline_child) {
      out_ += '\n';
    }

    const std::size_t before = out_.size();

    if (auto result = render_children(node); !result) {
      return result;
    }

    if (inline_child && out_.size() > before && out_.back() == '\n') {
      out_.pop_back();
    }

    out_ += "</";
    out_ += node.tag;
    out_ += ">\n";

    return {};
  }

  // Literal text in a template is written as the author wrote it, markup and
  // all. An interpolated value is not the author's markup, so it escapes. In an
  // attribute the whole result is escaped afterward, so the holes are
  // left alone there.
  static std::string escaped_copy(std::string_view value) {
    std::string out;
    escape(out, value);

    return out;
  }

  // A named argument by name, since render's arguments are written in whatever
  // order reads best.
  static const expression::Node* named(const expression::Node& call, std::string_view name) {
    for (const auto& argument : call.arguments) {
      if (argument->kind == expression::NodeKind::named_argument && argument->text == name) {
        return argument->target.get();
      }
    }

    return nullptr;
  }

  // Returns nothing when the call is not one the engine answers itself, so an
  // ordinary view function still reaches the evaluator.
  std::optional<std::expected<std::string, ParseError>> render_engine_call(const expression::Node& node) {
    if (node.kind != expression::NodeKind::call || node.target->kind != expression::NodeKind::name) {
      return std::nullopt;
    }

    const std::string& name = node.target->text;

    if (name == "render") {
      return render_partial_call(node);
    }

    if (name == "cache-fragment" || name == "cache_fragment") {
      return render_cached_fragment(node);
    }

    return std::nullopt;
  }

  std::expected<std::string, ParseError> render_partial_call(const expression::Node& node) {
    const expression::Node* partial_name = named(node, "partial");

    if (partial_name == nullptr || options_.partial == nullptr) {
      return std::unexpected(ParseError{"render needs a :partial to render", node.line, node.column});
    }

    auto resolved = expression::evaluate(*partial_name, context_);

    if (!resolved) {
      return std::unexpected(resolved.error());
    }

    const std::string target(resolved->as_string());
    const Template* partial = options_.partial(target);

    if (partial == nullptr) {
      return std::unexpected(
        ParseError{std::format("no such partial '{}'", target), node.line, node.column});
    }

    if (++depth_ > max_depth) {
      --depth_;

      return std::unexpected(
        ParseError{std::format("partials nested too deeply at '{}'", target), node.line, node.column});
    }

    // Arguments are evaluated against the locals in scope now, before those
    // locals are cleared for the partial. A partial that renders itself passes
    // its own loop variable down, so reading the arguments afterward would find
    // it already gone.
    Value locals_value;
    bool has_locals = false;

    if (const expression::Node* locals = named(node, "locals")) {
      auto value = expression::evaluate(*locals, context_);

      if (!value) {
        --depth_;

        return std::unexpected(value.error());
      }

      locals_value = std::move(*value);
      has_locals = true;
    }

    const expression::Node* collection = named(node, "collection");
    Value items;

    if (collection != nullptr) {
      auto value = expression::evaluate(*collection, context_);

      if (!value) {
        --depth_;

        return std::unexpected(value.error());
      }

      items = std::move(*value);
    }

    std::string as = "item";

    if (const expression::Node* alias = named(node, "as")) {
      auto value = expression::evaluate(*alias, context_);

      if (!value) {
        --depth_;

        return std::unexpected(value.error());
      }

      as = std::string(value->as_string());
    }

    auto saved = context_.take_locals();

    const auto restore = [&](std::vector<std::pair<std::string, Value>> locals) {
      context_.restore_locals(std::move(locals));
      --depth_;
    };

    if (has_locals) {
      for (const auto& member : locals_value.members()) {
        context_.set_local(member.first, member.second);
      }
    }

    if (collection == nullptr) {
      Renderer nested(context_, options_);
      nested.depth_ = depth_;

      auto rendered = nested.run(partial->root());

      restore(std::move(saved));

      return rendered;
    }

    std::string out;

    for (const Value& item : items.items()) {
      context_.set_local(as, item);

      Renderer nested(context_, options_);
      nested.depth_ = depth_;

      auto rendered = nested.run(partial->root());

      if (!rendered) {
        restore(std::move(saved));

        return rendered;
      }

      out += *rendered;
    }

    restore(std::move(saved));

    return out;
  }

  // The one deferred form: the block is rendered, and what it read decides
  // whether the result can stand in for the next page's.
  std::expected<std::string, ParseError> render_cached_fragment(const expression::Node& node) {
    if (node.arguments.size() < 2) {
      return std::unexpected(ParseError{"cache-fragment needs a name and a block", node.line, node.column});
    }

    const expression::Node& block = *node.arguments.back();
    const expression::Node& body = block.kind == expression::NodeKind::block ? *block.target : block;

    if (options_.fragments == nullptr) {
      auto rendered = render_fragment_body(body);

      return rendered;
    }

    // Two fragments that happen to read the same values are still two
    // fragments, so where the fragment is written is part of its key. The
    // compiled block identifies it: templates are compiled once, so its
    // address is stable for the build and unique to this cache-fragment.
    const void* site = &body;
    const std::string prefix = std::format("{:x}|", reinterpret_cast<std::uintptr_t>(site));

    // What it read last time, resolved against this page, is this page's key.
    // Working it out this way is what saves the work, since the fragment is not
    // rendered first and then thrown away.
    if (const std::vector<std::string>* names = options_.fragments->reads(site)) {
      if (const auto replayed = context_.replay(*names)) {
        if (const std::string* cached = options_.fragments->find(prefix + *replayed)) {
          return *cached;
        }
      }
    }

    context_.begin_recording();

    auto rendered = render_fragment_body(body);

    const std::string key = prefix + context_.end_recording();

    if (!rendered) {
      return rendered;
    }

    options_.fragments->note_render();

    if (context_.replayable()) {
      options_.fragments->remember_reads(site, context_.read_names());
    }

    if (const std::string* cached = options_.fragments->find(key)) {
      return *cached;
    }

    options_.fragments->store(key, *rendered);

    return rendered;
  }

  std::expected<std::string, ParseError> render_fragment_body(const expression::Node& body) {
    if (auto handled = render_engine_call(body); handled.has_value()) {
      return *handled;
    }

    auto value = expression::evaluate(body, context_);

    if (!value) {
      return std::unexpected(value.error());
    }

    return to_text(*value);
  }

  std::expected<std::string, ParseError> interpolate(const Interpolated& parts, bool escape_holes) {
    std::string out;

    for (const Segment& segment : parts) {
      if (segment.hole == nullptr) {
        out += segment.literal;
        continue;
      }

      auto value = expression::evaluate(*segment.hole, context_);

      if (!value) {
        return std::unexpected(value.error());
      }

      if (escape_holes) {
        escape(out, to_text(*value));
      } else {
        out += to_text(*value);
      }
    }

    return out;
  }

  static constexpr int max_depth = 32;

  ViewContext& context_;
  const RenderOptions& options_;
  std::string out_;
  int depth_ = 0;
};

}  // namespace

std::expected<std::string, ParseError> render(const Template& compiled, ViewContext& context,
                                              const RenderOptions& options) {
  Renderer renderer(context, options);

  return renderer.run(compiled.root());
}

}  // namespace blogin::haml
