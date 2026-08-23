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

      // A list interrupts a paragraph only when the list starts at one and its
      // first item carries something.
      spec::it("ends at a following list that starts at one", [] {
        expect(to_html("text\n- item")).to_eq("<p>text</p>\n<ul>\n<li>item</li>\n</ul>\n");
      });

      spec::it("ends at a following ordered list that starts at one", [] {
        expect(to_html("text\n1. item")).to_eq("<p>text</p>\n<ol>\n<li>item</li>\n</ol>\n");
      });

      spec::it("keeps a following ordered list that starts at two in the paragraph", [] {
        expect(to_html("text\n2. item")).to_eq("<p>text\n2. item</p>\n");
      });

      // A bullet that is not one of the setext underlines, so what is under
      // test is the empty item rather than the heading.
      spec::it("keeps a following empty list item in the paragraph", [] {
        expect(to_html("text\n*")).to_eq("<p>text\n*</p>\n");
      });

      spec::it("ends at a following block quote", [] {
        expect(to_html("text\n> quoted")).to_eq("<p>text</p>\n<blockquote>\n<p>quoted</p>\n</blockquote>\n");
      });

      // A line that continues a paragraph whose container did not match is a
      // lazy continuation, and a block quote is a block, so it is not one.
      spec::it("ends a list item's paragraph at a block quote on the next line", [] {
        expect(to_html("- foo\n> bar"))
          .to_eq("<ul>\n<li>foo</li>\n</ul>\n<blockquote>\n<p>bar</p>\n</blockquote>\n");
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

      // Whether an underscore closes emphasis depends on what follows it being
      // punctuation, and an en dash is punctuation as a code point rather than
      // as any one of the three bytes that spell it.
      spec::it("closes underscore emphasis against a multi-byte punctuation mark", [] {
        expect(to_html("_foo_\xe2\x80\x93" "bar")).to_eq("<p><em>foo</em>\xe2\x80\x93" "bar</p>\n");
      });
    });

    spec::context("a link destination in angle brackets", [] {
      spec::it("reads a plain one", [] {
        expect(to_html("[a](<b>)")).to_contain(R"(href="b")");
      });

      // Inside the brackets, a backslash escape lets a paren be part of the url.
      spec::it("reads an escaped punctuation mark inside the brackets", [] {
        expect(to_html(R"([a](<b\)c>))")).to_contain(R"(href="b)c")");
      });

      // An unescaped `<` cannot appear in the destination, so this is not one.
      spec::it("leaves a second opening bracket as text", [] {
        expect(to_html("[a](<b<c>)")).not_to_contain("href=");
      });
    });

    spec::context("a link title on the line after its destination", [] {
      spec::it("reads the title", [] {
        expect(to_html("[a](/url\n\"titled\")")).to_contain(R"(title="titled")");
      });
    });

    // A raw block of one of these four runs to its own closing tag, and the
    // markdown after it is markdown again.
    spec::context("lists", [] {
      // The rules that hold a list back from interrupting a paragraph do not
      // apply where there is no paragraph to interrupt.
      spec::it("opens an ordered list that starts at something other than one", [] {
        expect(to_html("2. item")).to_eq("<ol start=\"2\">\n<li>item</li>\n</ol>\n");
      });

      spec::it("opens a list whose first item is empty", [] {
        expect(to_html("*")).to_eq("<ul>\n<li></li>\n</ul>\n");
      });
    });

    spec::context("a raw html block", [] {
      spec::it("ends a script block at its closing tag", [] {
        expect(to_html("<script>\nkeep\n</script>\n\nafter"))
          .to_eq("<script>\nkeep\n</script>\n<p>after</p>\n");
      });

      spec::it("ends a pre block at its closing tag", [] {
        expect(to_html("<pre>\nkeep\n</pre>\n\nafter")).to_eq("<pre>\nkeep\n</pre>\n<p>after</p>\n");
      });

      spec::it("ends a style block at its closing tag", [] {
        expect(to_html("<style>\nkeep\n</style>\n\nafter")).to_eq("<style>\nkeep\n</style>\n<p>after</p>\n");
      });

      spec::it("ends a textarea block at its closing tag", [] {
        expect(to_html("<textarea>\nkeep\n</textarea>\n\nafter"))
          .to_eq("<textarea>\nkeep\n</textarea>\n<p>after</p>\n");
      });
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
