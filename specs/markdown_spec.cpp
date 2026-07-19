#include <string>

#include "arena.h"
#include "html.h"
#include "markdown.h"
#include "support/spec.h"

using blogin::Arena;
using blogin::Source;
using spec::expect;

namespace {

std::string to_html(std::string markdown) {
  const Source source(std::move(markdown));

  Arena arena;

  return blogin::render_html(blogin::parse_markdown(arena, source));
}

}  // namespace

SPEC {
  spec::describe("markdown", [] {
    spec::context("paragraphs", [] {
      spec::it("wraps a line of text", [] { expect(to_html("hello")).to_eq("<p>hello</p>\n"); });

      spec::it("starts a new paragraph after a blank line", [] {
        expect(to_html("one\n\ntwo")).to_eq("<p>one</p>\n<p>two</p>\n");
      });

      spec::it("keeps consecutive lines in one paragraph", [] {
        expect(to_html("one\ntwo")).to_eq("<p>one\ntwo</p>\n");
      });

      spec::it("ends at a following heading", [] {
        expect(to_html("text\n# title")).to_eq("<p>text</p>\n<h1>title</h1>\n");
      });
    });

    spec::context("headings", [] {
      spec::it("renders at the marked level", [] { expect(to_html("### deep")).to_eq("<h3>deep</h3>\n"); });

      spec::it("treats a run of hashes without a space as text", [] {
        expect(to_html("###deep")).to_eq("<p>###deep</p>\n");
      });

      spec::it("treats more than six hashes as text", [] {
        expect(to_html("####### too deep")).to_eq("<p>####### too deep</p>\n");
      });
    });

    spec::context("inline markup", [] {
      spec::it("renders emphasis", [] {
        expect(to_html("an *emphatic* word")).to_eq("<p>an <em>emphatic</em> word</p>\n");
      });

      spec::it("renders strong", [] {
        expect(to_html("a **bold** word")).to_eq("<p>a <strong>bold</strong> word</p>\n");
      });

      spec::it("nests emphasis inside strong", [] {
        expect(to_html("**bold *and* more**")).to_eq("<p><strong>bold <em>and</em> more</strong></p>\n");
      });

      spec::it("leaves an unmatched marker as text", [] { expect(to_html("2 * 3")).to_eq("<p>2 * 3</p>\n"); });
    });

    spec::context("escaping", [] {
      spec::it("escapes the html special characters", [] {
        expect(to_html("a < b & c > d")).to_eq("<p>a &lt; b &amp; c &gt; d</p>\n");
      });
    });

    spec::context("empty input", [] {
      spec::it("renders nothing for an empty document", [] { expect(to_html("")).to_eq(""); });

      spec::it("renders nothing for whitespace alone", [] { expect(to_html("   \n\n  \n")).to_eq(""); });
    });
  });
}
