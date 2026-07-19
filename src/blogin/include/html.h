#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "framework.h"
#include "markdown.h"
#include "shortcode.h"
#include "toc.h"

namespace blogin {

struct RenderOptions {
  Framework framework = Framework::profile("none");
  bool highlight = false;
  const ShortcodeRegistry* shortcodes = nullptr;

  // Blogin gives every heading an id and a link to itself. Off by default,
  // because the conformance suite compares against plain CommonMark output.
  bool heading_anchors = false;
};

// One pass over the tree produces all three: the HTML, the stripped text that
// summaries and the search index read, and the headings a table of contents is
// built from. Walking three times would cost three times as much and let the
// three drift apart.
struct RenderResult {
  std::string html;
  std::string text;
  std::vector<Heading> headings;
};

RenderResult render_document(const Node* node, const RenderOptions& options = {});

void append_escaped(std::string& out, std::string_view text);

void append_attribute_escaped(std::string& out, std::string_view text);

void append_url_escaped(std::string& out, std::string_view url);

// Plain HTML with no framework classes, highlighting, or shortcodes. Used where
// only the markup matters, such as the conformance suite.
void render_html(std::string& out, const Node* node);

std::string render_html(const Node* node);

}  // namespace blogin
