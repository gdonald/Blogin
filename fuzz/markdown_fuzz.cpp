#include <cstddef>
#include <cstdint>
#include <string>

#include "arena.h"
#include "html.h"
#include "markdown.h"
#include "require.h"

// The parser reads untrusted input and every node holds a view into the source
// buffer, so a scan running past the end is the failure this target hunts.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  blogin::Arena arena;

  const blogin::Source source(input);
  const blogin::Node* tree = blogin::parse_markdown(arena, source);

  std::string html;
  blogin::render_html(html, tree);

  // The other renderer walks the same tree and does more with it: framework
  // classes, heading ids, and the stripped text the search index reads.
  const blogin::RenderResult full = blogin::render_document(tree, {.heading_anchors = true});

  // Parsing is a pure function of the bytes. A second pass that disagrees means
  // state survived the first, through the arena or through something static.
  blogin::Arena second_arena;

  const blogin::Source again(input);
  std::string repeat;
  blogin::render_html(repeat, blogin::parse_markdown(second_arena, again));

  FUZZ_REQUIRE(html == repeat);

  // Every heading the table of contents names has to be findable by the id the
  // renderer gave it, or the anchor links point at nothing.
  for (const blogin::Heading& heading : full.headings) {
    FUZZ_REQUIRE(heading.id.empty() || full.html.contains(heading.id));
  }

  return 0;
}
