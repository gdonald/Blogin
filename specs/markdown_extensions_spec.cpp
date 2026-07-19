#include <string>

#include "arena.h"
#include "html.h"
#include "markdown.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::string render(const std::string& markdown) {
  blogin::Arena arena;

  return blogin::render_html(blogin::parse_markdown(arena, markdown));
}

}  // namespace

// Extensions on top of CommonMark, following GitHub Flavored Markdown where it
// defines them.
SPEC {
  spec::describe("markdown extensions", [] {
    spec::context("strikethrough", [] {
      spec::it("marks a doubled tilde run", [] {
        expect(render("~~gone~~")).to_eq("<p><del>gone</del></p>\n");
      });

      spec::it("marks a single tilde run", [] {
        expect(render("~gone~")).to_eq("<p><del>gone</del></p>\n");
      });

      spec::it("leaves an unmatched tilde as text", [] {
        expect(render("a ~ b")).to_eq("<p>a ~ b</p>\n");
      });

      spec::it("leaves a run of more than two tildes as text", [] {
        expect(render("~~~a~~~")).to_contain("~~~");
      });

      spec::it("nests emphasis inside it", [] {
        expect(render("~~a *b* c~~")).to_eq("<p><del>a <em>b</em> c</del></p>\n");
      });

      spec::it("nests inside emphasis", [] {
        expect(render("*a ~~b~~ c*")).to_eq("<p><em>a <del>b</del> c</em></p>\n");
      });
    });

    spec::context("task list items", [] {
      spec::it("renders an unchecked box", [] {
        expect(render("- [ ] todo")).to_contain(R"(<input type="checkbox" disabled="" /> todo)");
      });

      spec::it("renders a checked box", [] {
        expect(render("- [x] done")).to_contain("checked=\"\"");
      });

      spec::it("accepts an uppercase marker", [] {
        expect(render("- [X] done")).to_contain("checked=\"\"");
      });

      spec::it("leaves a plain item alone", [] {
        expect(render("- plain")).not_to_contain("checkbox");
      });

      spec::it("leaves a bracket that is not a marker alone", [] {
        expect(render("- [y] no")).not_to_contain("checkbox");
      });

      spec::it("works in an ordered list", [] {
        expect(render("1. [ ] todo")).to_contain("checkbox");
      });
    });

    spec::context("footnotes", [] {
      auto html = spec::let([] {
        return render("A note[^a] and another[^b], then a repeat[^a].\n\n[^a]: First.\n[^b]: Second.\n");
      });

      spec::it("numbers references in the order they appear", [=] {
        spec::aggregate_failures([=] {
          expect(html()).to_contain(">1</a></sup> and another");
          expect(html()).to_contain(">2</a></sup>, then");
        });
      });

      spec::it("gives a repeated reference the same number", [=] {
        expect(html()).to_contain("id=\"fnref-a-2\">1</a>");
      });

      spec::it("collects the bodies into a section", [=] {
        expect(html()).to_contain("<section class=\"footnotes\">");
      });

      spec::it("keeps the definition out of the body text", [=] {
        expect(html()).not_to_contain("<p>[^a]: First.</p>");
      });

      spec::it("links a reference to its body", [=] { expect(html()).to_contain("href=\"#fn-a\""); });

      spec::it("links a body back to its reference", [=] { expect(html()).to_contain("href=\"#fnref-a\""); });

      spec::it("parses markup inside a body", [] {
        expect(render("x[^a]\n\n[^a]: with *emphasis*\n")).to_contain("<em>emphasis</em>");
      });

      spec::it("leaves a reference with no definition as text", [] {
        expect(render("x[^missing]\n")).not_to_contain("<section");
      });
    });

    spec::context("definition lists", [] {
      spec::it("makes a term and a definition", [] {
        expect(render("Term\n: meaning\n")).to_eq("<dl>\n<dt>Term</dt>\n<dd>meaning</dd>\n</dl>\n");
      });

      spec::it("accepts several definitions for one term", [] {
        expect(render("Term\n: one\n: two\n")).to_contain("<dd>one</dd>\n<dd>two</dd>");
      });

      spec::it("accepts several terms in one list", [] {
        const std::string html = render("A\n: one\nB\n: two\n");

        spec::aggregate_failures([&] {
          expect(html).to_contain("<dt>A</dt>");
          expect(html).to_contain("<dt>B</dt>");
        });
      });

      spec::it("parses markup in a definition", [] {
        expect(render("Term\n: with *emphasis*\n")).to_contain("<em>emphasis</em>");
      });

      spec::it("leaves a colon that starts no definition alone", [] {
        expect(render("just: text\n")).to_eq("<p>just: text</p>\n");
      });
    });

    spec::context("math", [] {
      spec::it("marks an inline span", [] {
        expect(render("$a+b$")).to_eq("<p><span class=\"math math-inline\">a+b</span></p>\n");
      });

      spec::it("marks a display span", [] {
        expect(render("$$x^2$$")).to_contain("math math-display");
      });

      // A dollar followed by a digit is a price, not an opening delimiter.
      spec::it("leaves a price alone", [] { expect(render("costs $5 today")).to_contain("$5"); });

      spec::it("leaves an unmatched dollar alone", [] { expect(render("a $ b")).to_contain("$"); });

      spec::it("escapes markup inside", [] {
        expect(render("$a<b$")).to_contain("a&lt;b");
      });
    });

    spec::context("shortcodes", [] {
      spec::it("recognises one alone on a line", [] {
        expect(render("{{< youtube id=\"abc\" >}}")).to_contain("youtube");
      });

      // Expansion belongs to the shortcode registry. Until then the source is
      // shown rather than injected as markup.
      spec::it("does not inject it as markup", [] {
        expect(render("{{< raw >}}")).not_to_contain("<raw");
      });

      spec::it("leaves an unclosed shortcode as a paragraph", [] {
        expect(render("{{< broken")).to_contain("<p>");
      });
    });

    spec::context("attribute blocks", [] {
      spec::it("adds a class to a link", [] {
        expect(render("[a](/u){.fancy}")).to_contain("class=\"fancy\"");
      });

      spec::it("combines several classes", [] {
        expect(render("[a](/u){.one .two}")).to_contain("class=\"one two\"");
      });

      spec::it("adds an id", [] { expect(render("[a](/u){#main}")).to_contain("id=\"main\""); });

      spec::it("adds a named attribute", [] {
        expect(render("[a](/u){data-x=\"1\"}")).to_contain("data-x=\"1\"");
      });

      spec::it("adds an unquoted attribute", [] {
        expect(render("[a](/u){width=50}")).to_contain("width=\"50\"");
      });

      spec::it("adds a class to an image", [] {
        expect(render("![a](/u.png){.wide}")).to_contain("class=\"wide\"");
      });

      spec::it("leaves a brace that is not an attribute block alone", [] {
        expect(render("[a](/u) {not attributes}")).to_contain("{not attributes}");
      });
    });

    spec::context("tables", [] {
      auto simple = spec::let([] { return render("| a | b |\n| - | - |\n| 1 | 2 |\n"); });

      spec::it("emits a table", [=] { expect(simple()).to_contain("<table>"); });

      spec::it("puts the first row in a head", [=] { expect(simple()).to_contain("<thead>"); });

      spec::it("uses header cells in the head", [=] { expect(simple()).to_contain("<th>a</th>"); });

      spec::it("uses data cells in the body", [=] { expect(simple()).to_contain("<td>1</td>"); });

      spec::it("closes the body", [=] { expect(simple()).to_contain("</tbody>"); });

      spec::it("reads left alignment", [] {
        expect(render("| a |\n|:--|\n| 1 |\n")).to_contain("<th align=\"left\">a</th>");
      });

      spec::it("reads centre alignment", [] {
        expect(render("| a |\n|:-:|\n| 1 |\n")).to_contain("align=\"center\"");
      });

      spec::it("reads right alignment", [] {
        expect(render("| a |\n|--:|\n| 1 |\n")).to_contain("align=\"right\"");
      });

      spec::it("leaves an unaligned column without an attribute", [] {
        expect(render("| a |\n|---|\n| 1 |\n")).to_contain("<th>a</th>");
      });

      spec::it("parses inline markup inside a cell", [] {
        expect(render("| a |\n|---|\n| *x* |\n")).to_contain("<em>x</em>");
      });

      spec::it("pads a short row to the declared width", [] {
        expect(render("| a | b |\n| - | - |\n| 1 |\n")).to_contain("<td></td>");
      });

      spec::it("ignores cells past the declared width", [] {
        const std::string html = render("| a |\n| - |\n| 1 | 2 |\n");

        expect(html).not_to_contain("<td>2</td>");
      });

      spec::it("reads an escaped pipe as content", [] {
        expect(render("| a |\n|---|\n| x \\| y |\n")).to_contain("x | y");
      });

      spec::it("works without outer pipes", [] {
        expect(render("a | b\n--- | ---\n1 | 2\n")).to_contain("<table>");
      });

      spec::it("ends at a blank line", [] {
        expect(render("| a |\n|---|\n| 1 |\n\nafter\n")).to_contain("<p>after</p>");
      });

      // A row of hyphens with no pipes is a setext underline, not a table.
      spec::it("does not take a setext underline for a delimiter row", [] {
        expect(render("Title\n---\n")).to_eq("<h2>Title</h2>\n");
      });

      spec::it("needs the column counts to agree", [] {
        expect(render("| a | b |\n| - |\n")).not_to_contain("<table>");
      });

      spec::it("does not start from a multi-line paragraph", [] {
        expect(render("one\ntwo\n| - |\n")).not_to_contain("<table>");
      });

      spec::it("needs something in every delimiter cell", [] {
        expect(render("| a | b |\n| - |  |\n| 1 | 2 |\n")).not_to_contain("<table>");
      });

      spec::it("needs a hyphen in the delimiter row", [] {
        expect(render("| a | b |\n| x | y |\n")).not_to_contain("<table>");
      });
    });

    // The corners of each extension's own matcher, which the conformance suite
    // has no reason to reach.
    spec::context("markers that look like an extension and are not", [] {
      spec::it("leaves a task marker with no space after it alone", [] {
        expect(render("- [ ]todo")).not_to_contain("checkbox");
      });

      spec::it("leaves an empty shortcode name alone", [] {
        expect(render("{{< >}}")).not_to_contain("<iframe");
      });

      spec::it("leaves an empty line where a thematic break would go", [] {
        expect(render("a\n\n\nb")).not_to_contain("<hr");
      });

      spec::it("reads a tab as indentation to the next stop", [] {
        expect(render("-\tone")).to_contain("<li>");
      });

      // Every width a UTF-8 sequence comes in, since flanking is decided over
      // characters rather than bytes.
      spec::it("reads a four-byte character beside an emphasis marker", [] {
        expect(render("*\U0001F600*")).to_contain("<em>");
      });

      spec::it("reads a three-byte character beside an emphasis marker", [] {
        expect(render("*\u4E16*")).to_contain("<em>");
      });

      spec::it("leaves a truncated sequence alone", [] {
        expect(render(std::string("*a\xC3") + "*")).to_contain("<em>");
      });

      spec::it("leaves a stray continuation byte alone", [] {
        expect(render(std::string("*a\x80") + "b*")).to_contain("<em>");
      });

      spec::it("decodes a numeric character reference above the basic plane", [] {
        expect(render("&#128512;")).to_contain("\U0001F600");
      });

      spec::it("decodes a numeric character reference in the basic plane", [] {
        expect(render("&#19990;")).to_contain("\u4E16");
      });

      spec::it("leaves an entity with nothing in it alone", [] {
        expect(render("&;")).to_contain("&amp;;");
      });

      spec::it("leaves an entity with no semicolon alone", [] {
        expect(render("&amp")).to_contain("&amp;amp");
      });

      spec::it("leaves a run too long to be an entity alone", [] {
        expect(render("&" + std::string(40, 'a') + ";")).to_contain("&amp;");
      });

      spec::it("leaves an unclosed comment as text", [] {
        expect(render("a <!-- b")).to_contain("&lt;!--");
      });

      spec::it("leaves an unclosed processing instruction as text", [] {
        expect(render("a <?php b")).to_contain("&lt;?php");
      });

      spec::it("leaves an unclosed cdata block as text", [] {
        expect(render("a <![CDATA[ b")).to_contain("&lt;![CDATA[");
      });

      spec::it("leaves an unclosed declaration as text", [] {
        expect(render("a <!DOCTYPE b")).to_contain("&lt;!DOCTYPE");
      });

      spec::it("leaves an attribute with an unclosed quote alone", [] {
        expect(render("a <b c=\"d> e")).to_contain("&lt;b");
      });

      spec::it("leaves an attribute with no value alone", [] {
        expect(render("a <b c=> d")).to_contain("&lt;b");
      });

      spec::it("leaves an attribute value that runs to the end alone", [] {
        expect(render("a <b c=")).to_contain("&lt;b");
      });

      spec::it("leaves a footnote reference with no label alone", [] {
        expect(render("a[^] b")).not_to_contain("footnote");
      });

      spec::it("leaves a footnote reference with no closing bracket alone", [] {
        expect(render("a[^note b")).not_to_contain("footnote");
      });

      spec::it("leaves inline math spanning a blank line alone", [] {
        expect(render("$a\n\nb$")).not_to_contain("math");
      });

      spec::it("leaves inline math ending in a space alone", [] {
        expect(render("$a $")).not_to_contain("math");
      });

      spec::it("leaves an unclosed attribute block alone", [] {
        expect(render("[text](/url){.cls")).to_contain("{.cls");
      });

      spec::it("drops the indentation after a hard break", [] {
        expect(render("one  \n    two")).to_contain("<br />");
      });

      spec::it("reads an attribute block written with no value", [] {
        expect(render("[text](/url){.cls key}")).to_contain("class=\"cls\"");
      });

      spec::it("reads an attribute block with an empty marker", [] {
        expect(render("[text](/url){. #id}")).to_contain("id=\"id\"");
      });

      spec::it("leaves a tilde run of three alone", [] {
        expect(render("~~~gone~~~")).not_to_contain("<del>");
      });

      spec::it("reads a reference definition with an angled destination", [] {
        expect(render("[text][a]\n\n[a]: <> \"Title\"\n")).to_contain("title=\"Title\"");
      });

      spec::it("leaves an unclosed angled destination alone", [] {
        expect(render("[text][a]\n\n[a]: <one\n")).to_contain("[text][a]");
      });

      spec::it("keeps the first of two definitions of the same label", [] {
        expect(render("[text][a]\n\n[a]: /one\n[a]: /two\n")).to_contain("/one");
      });

      spec::it("leaves a definition whose title sits alone on the next line alone", [] {
        expect(render("[text][a]\n\n[a]: /one\n\"Title\"\n")).to_contain("/one");
      });

      spec::it("leaves a footnote definition with no label alone", [] {
        expect(render("[^]: nothing\n")).not_to_contain("<section");
      });

      spec::it("leaves a footnote definition with no colon alone", [] {
        expect(render("[^a] nothing\n")).not_to_contain("<section");
      });

      spec::it("writes no footnote section when nothing referenced one", [] {
        expect(render("Text.\n\n[^a]: Unused.\n")).not_to_contain("<section");
      });

      spec::it("closes every open container at the end of a document", [] {
        expect(render("> - one\n>   - two")).to_contain("</blockquote>");
      });
    });
  });
}
