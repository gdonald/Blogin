#include "toc.h"

#include <vector>

namespace blogin::toc {
namespace {

void escape(std::string& out, std::string_view text) {
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

void render_into(std::string& out, const std::vector<Entry>& entries) {
  if (entries.empty()) {
    return;
  }

  out += "<ul>";

  for (const Entry& entry : entries) {
    out += "<li><a href=\"#";
    escape(out, entry.id);
    out += "\">";
    escape(out, entry.title);
    out += "</a>";

    render_into(out, entry.children);

    out += "</li>";
  }

  out += "</ul>";
}

}  // namespace

std::vector<Entry> build(const std::vector<Heading>& headings) {
  std::vector<Entry> roots;

  // Each entry is reached by the path of levels above it, so a deeper heading
  // lands inside the last shallower one.
  std::vector<int> levels;

  for (const Heading& heading : headings) {
    while (!levels.empty() && levels.back() >= heading.level) {
      levels.pop_back();
    }

    std::vector<Entry>* target = &roots;

    for (std::size_t depth = 0; depth < levels.size(); ++depth) {
      if (target->empty()) {
        break;
      }

      target = &target->back().children;
    }

    target->push_back(Entry{heading.text, heading.id, heading.level, {}});
    levels.push_back(heading.level);
  }

  return roots;
}

std::string render(const std::vector<Entry>& entries) {
  std::string out;

  render_into(out, entries);

  return out;
}

}  // namespace blogin::toc
