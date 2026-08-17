#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include "arena.h"
#include "filters.h"
#include "framework.h"
#include "highlight.h"
#include "html.h"
#include "markdown.h"
#include "metrics.h"
#include "shortcode.h"
#include "slug.h"
#include "style.h"
#include "summary.h"
#include "support/spec.h"
#include "toc.h"

using spec::expect;

namespace {

blogin::RenderResult render(const std::string& markdown, const blogin::RenderOptions& options) {
  blogin::Arena arena;

  return blogin::render_document(blogin::parse_markdown(arena, markdown), options);
}

blogin::RenderResult render(const std::string& markdown) {
  return render(markdown, blogin::RenderOptions{});
}

}  // namespace

SPEC {
  spec::describe("slugs", [] {
    spec::it("lowercases", [] { expect(blogin::slug::slugify("Hello")).to_eq("hello"); });

    spec::it("joins words with a hyphen", [] {
      expect(blogin::slug::slugify("Hello World")).to_eq("hello-world");
    });

    spec::it("collapses a run of separators", [] {
      expect(blogin::slug::slugify("a -- b")).to_eq("a-b");
    });

    spec::it("drops leading and trailing separators", [] {
      expect(blogin::slug::slugify("  Hi!  ")).to_eq("hi");
    });

    spec::it("keeps digits", [] { expect(blogin::slug::slugify("Top 10")).to_eq("top-10"); });

    spec::it("yields nothing for punctuation alone", [] { expect(blogin::slug::slugify("!!!")).to_eq(""); });

    // Dropping these would make "c++" and "c" the same tag, and one of them
    // would quietly get the other's page.
    spec::it("spells out a plus rather than dropping it", [] {
      expect(blogin::slug::slugify("C++")).to_eq("c-plus-plus");
    });

    spec::it("spells out a sharp rather than dropping it", [] {
      expect(blogin::slug::slugify("C#")).to_eq("c-sharp");
    });

    spec::it("spells out an ampersand rather than dropping it", [] {
      expect(blogin::slug::slugify("Rock & Roll")).to_eq("rock-and-roll");
    });

    spec::it("keeps a name distinct from the one it would collapse to", [] {
      expect(blogin::slug::slugify("C++")).not_to_eq(blogin::slug::slugify("C"));
    });

    spec::it("humanizes a hyphenated name", [] {
      expect(blogin::slug::humanize("my-first-post")).to_eq("My First Post");
    });

    spec::it("humanizes an underscored name", [] {
      expect(blogin::slug::humanize("my_post")).to_eq("My Post");
    });
  });

  spec::describe("metrics", [] {
    spec::it("counts words", [] { expect(blogin::metrics::word_count("one two three")).to_eq(std::size_t{3}); });

    spec::it("rounds reading time up", [] { expect(blogin::metrics::reading_time(201, 200)).to_eq(2); });

    spec::it("never reports less than a minute for a post with words", [] {
      expect(blogin::metrics::reading_time(1, 200)).to_eq(1);
    });

    spec::it("reports nothing for an empty post", [] { expect(blogin::metrics::reading_time(0)).to_eq(0); });
  });

  spec::describe("summaries", [] {
    spec::it("prefers an explicit summary", [] {
      expect(blogin::summary::choose("chosen", "excerpt", "body")).to_eq("chosen");
    });

    spec::it("falls back to the excerpt", [] {
      expect(blogin::summary::choose("", "excerpt", "body")).to_eq("excerpt");
    });

    spec::it("falls back to the opening block", [] {
      expect(blogin::summary::choose("", "", "first\n\nsecond")).to_eq("first");
    });

    spec::it("truncates on a word boundary", [] {
      expect(blogin::summary::truncate("one two three", 9)).to_eq("one two…");
    });

    spec::it("leaves a short string alone", [] {
      expect(blogin::summary::truncate("short", 20)).to_eq("short");
    });

    // Counting bytes would cut an accented character in half.
    spec::it("counts characters rather than bytes", [] {
      expect(blogin::summary::truncate("café société", 6)).to_eq("café…");
    });
  });

  spec::describe("filters", [] {
    spec::it("formats a date", [] {
      expect(blogin::filters::format_date("2024-03-07", "%B %e, %Y")).to_eq("March 7, 2024");
    });

    spec::it("passes through something that is not a date", [] {
      expect(blogin::filters::format_date("soon", "%Y")).to_eq("soon");
    });

    spec::it("groups by a field", [] {
      blogin::Value first = blogin::Value::object();
      first.set("month", blogin::Value("2024-01"));

      blogin::Value second = blogin::Value::object();
      second.set("month", blogin::Value("2024-02"));

      blogin::Value third = blogin::Value::object();
      third.set("month", blogin::Value("2024-01"));

      const auto groups = blogin::filters::group_by(blogin::Value::array({first, second, third}), "month");

      spec::aggregate_failures([&] {
        expect(groups.size()).to_eq(std::size_t{2});
        expect(groups[0].key).to_eq("2024-02");
        expect(groups[1].items.size()).to_eq(std::size_t{2});
      });
    });
  });

  spec::describe("highlighting", [] {
    spec::it("supports the languages it lists", [] {
      expect(blogin::highlight::languages().size()).to_eq(std::size_t{12});
    });

    spec::it("recognises a language it knows", [] {
      expect(blogin::highlight::supports("rust")).to_be_true();
    });

    spec::it("reads only the first word of an info string", [] {
      expect(blogin::highlight::supports("rust ignore")).to_be_true();
    });

    spec::it("does not recognise one it has no rules for", [] {
      expect(blogin::highlight::supports("brainfuck")).to_be_false();
    });

    spec::it("marks a keyword", [] {
      expect(blogin::highlight::render("fn main", "rust")).to_contain("<span class=\"hl-keyword\">fn</span>");
    });

    spec::it("marks a string", [] {
      expect(blogin::highlight::render("x = \"hi\"", "python")).to_contain("hl-string");
    });

    spec::it("marks a number", [] {
      expect(blogin::highlight::render("x = 42", "python")).to_contain("hl-number");
    });

    spec::it("marks a comment", [] {
      expect(blogin::highlight::render("# note", "python")).to_contain("hl-comment");
    });

    spec::it("escapes what it does not mark", [] {
      expect(blogin::highlight::render("a < b", "python")).to_contain("&lt;");
    });

    spec::it("leaves an unknown language escaped", [] {
      expect(blogin::highlight::render("a < b", "brainfuck")).to_eq("a &lt; b");
    });

    spec::context("in a rendered document", [] {
      auto options = spec::let([] {
        blogin::RenderOptions highlighted;
        highlighted.highlight = true;

        return highlighted;
      });

      spec::it("highlights a language it knows", [=] {
        expect(render("```rust\nfn main() {}\n```\n", options()).html).to_contain("hl-keyword");
      });

      // Otherwise a reader is left wondering why one block came out plain.
      spec::it("marks a block it could not highlight", [=] {
        expect(render("```brainfuck\n+++\n```\n", options()).html).to_contain("hl-plain");
      });

      spec::it("leaves blocks alone when highlighting is off", [] {
        expect(render("```rust\nfn main() {}\n```\n").html).not_to_contain("hl-keyword");
      });
    });
  });

  spec::describe("shortcodes", [] {
    spec::it("parses arguments in order", [] {
      const auto arguments = blogin::shortcode::parse_arguments(R"(id="abc" width="4")");

      spec::aggregate_failures([&] {
        expect(arguments.size()).to_eq(std::size_t{2});
        expect(arguments[0].name).to_eq("id");
        expect(arguments[0].value).to_eq("abc");
      });
    });

    spec::it("accepts single quotes", [] {
      const auto arguments = blogin::shortcode::parse_arguments("id='abc'");

      expect(arguments[0].value).to_eq("abc");
    });

    spec::it("ignores an argument with no value", [] {
      expect(blogin::shortcode::parse_arguments("bare").size()).to_eq(std::size_t{0});
    });

    spec::it("substitutes a placeholder", [] {
      expect(blogin::shortcode::render_template("<i>{{ id }}</i>",
                                                blogin::shortcode::parse_arguments("id=\"x\"")))
        .to_eq("<i>x</i>");
    });

    spec::it("escapes a substituted value", [] {
      expect(blogin::shortcode::render_template("{{ id }}", blogin::shortcode::parse_arguments("id=\"<b>\"")))
        .to_eq("&lt;b&gt;");
    });

    spec::it("writes nothing for an unknown placeholder", [] {
      expect(blogin::shortcode::render_template("{{ missing }}", {})).to_eq("");
    });

    spec::it("expands the youtube builtin", [] {
      expect(blogin::shortcode::expand_builtin("youtube", blogin::shortcode::parse_arguments("id=\"abc\"")))
        .to_contain("youtube.com/embed/abc");
    });

    spec::it("expands the figure builtin", [] {
      expect(blogin::shortcode::expand_builtin("figure",
                                               blogin::shortcode::parse_arguments(R"(src="/a.png" alt="a")")))
        .to_contain("<figure>");
    });

    spec::it("adds a caption only when given one", [] {
      expect(blogin::shortcode::expand_builtin("figure", blogin::shortcode::parse_arguments("src=\"/a.png\"")))
        .not_to_contain("figcaption");
    });

    // Anything after the last name is not an argument, so scanning stops there
    // rather than looping on characters that can never start one.
    spec::it("stops at trailing punctuation after the last argument", [] {
      expect(blogin::shortcode::parse_arguments(R"(id="abc" !!!)").size()).to_eq(std::size_t{1});
    });

    spec::it("still reads the argument before trailing punctuation", [] {
      expect(blogin::shortcode::parse_arguments(R"(id="abc" !!!)")[0].value).to_eq("abc");
    });

    spec::it("keeps an argument written with spaces around the equals", [] {
      expect(blogin::shortcode::parse_arguments("id = \"abc\"")[0].value).to_eq("abc");
    });

    spec::it("ignores an argument whose value is not quoted", [] {
      expect(blogin::shortcode::parse_arguments("id=abc").size()).to_eq(std::size_t{0});
    });

    spec::it("ignores an argument with nothing after the equals", [] {
      expect(blogin::shortcode::parse_arguments("id=").size()).to_eq(std::size_t{0});
    });

    spec::it("reads the last argument of a run", [] {
      expect(blogin::shortcode::parse_arguments(R"(a="1" b="2" c="3")").size()).to_eq(std::size_t{3});
    });

    spec::it("escapes every character markup would take differently", [] {
      expect(blogin::shortcode::render_template(
               "{{ id }}", blogin::shortcode::parse_arguments(R"(id="a&b<c>d'e")")))
        .to_eq("a&amp;b&lt;c&gt;d'e");
    });

    spec::it("escapes a quote in an attribute the builtins write", [] {
      expect(blogin::shortcode::expand_builtin(
               "figure", blogin::shortcode::parse_arguments(R"(src="/a.png" alt='say "hi"')")))
        .to_contain("&quot;");
    });

    spec::it("adds a caption when given one", [] {
      expect(blogin::shortcode::expand_builtin(
               "figure", blogin::shortcode::parse_arguments(R"(src="/a.png" caption="A cat")")))
        .to_contain("<figcaption>A cat</figcaption>");
    });

    spec::it("leaves an unclosed placeholder alone", [] {
      expect(blogin::shortcode::render_template("{{ id", {})).to_eq("{{ id");
    });

    spec::it("leaves a lone brace alone", [] {
      expect(blogin::shortcode::render_template("{ id }", {})).to_eq("{ id }");
    });

    // A site's own shortcodes, read from its shortcodes directory.
    spec::context("a registry loaded from disk", [] {
      auto directory = spec::let([] {
        const std::filesystem::path root = spec::scratch_directory("shortcodes");

        std::filesystem::create_directories(root);

        std::ofstream(root / "callout.html", std::ios::binary) << "<aside>{{ text }}</aside>";

        // Only .html files are shortcodes, whatever else the directory holds.
        std::ofstream(root / "notes.txt", std::ios::binary) << "not a shortcode";

        return root;
      });

      spec::it("finds a template the site wrote", [=] {
        expect(blogin::ShortcodeRegistry::load(directory()).contains("callout")).to_be_true();
      });

      spec::it("finds a builtin as well", [=] {
        expect(blogin::ShortcodeRegistry::load(directory()).contains("youtube")).to_be_true();
      });

      spec::it("finds nothing under a name nobody wrote", [=] {
        expect(blogin::ShortcodeRegistry::load(directory()).contains("mystery")).to_be_false();
      });

      spec::it("takes only html files", [=] {
        expect(blogin::ShortcodeRegistry::load(directory()).contains("notes")).to_be_false();
      });

      spec::it("expands the site's own template", [=] {
        expect(blogin::ShortcodeRegistry::load(directory()).expand("callout", R"(text="hi")", ""))
          .to_eq("<aside>hi</aside>");
      });

      spec::it("expands a builtin through the registry", [=] {
        expect(blogin::ShortcodeRegistry::load(directory()).expand("youtube", R"(id="abc")", ""))
          .to_contain("<iframe");
      });

      // Showing what was written beats emitting nothing.
      spec::it("gives back the source of one it does not know", [=] {
        expect(blogin::ShortcodeRegistry::load(directory()).expand("mystery", "", "{{< mystery >}}"))
          .to_contain("mystery");
      });
    });

    spec::context("in a rendered document", [] {
      auto registry = spec::let([] { return blogin::ShortcodeRegistry::load("/nonexistent"); });

      spec::it("expands a builtin", [=] {
        blogin::RenderOptions options;
        options.shortcodes = &registry();

        expect(render("{{< youtube id=\"abc\" >}}", options).html).to_contain("<iframe");
      });

      // Showing what was written beats emitting nothing.
      spec::it("shows an unknown shortcode as its own source", [=] {
        blogin::RenderOptions options;
        options.shortcodes = &registry();

        expect(render("{{< mystery >}}", options).html).to_contain("mystery");
      });

      spec::it("does not inject an unknown shortcode as markup", [=] {
        blogin::RenderOptions options;
        options.shortcodes = &registry();

        expect(render("{{< mystery >}}", options).html).not_to_contain("<mystery");
      });
    });
  });

  spec::describe("the render result", [] {
    spec::it("collects headings", [] {
      expect(render("# One\n\n## Two\n").headings.size()).to_eq(std::size_t{2});
    });

    spec::it("records a heading level", [] { expect(render("## Two\n").headings[0].level).to_eq(2); });

    spec::it("records a heading id", [] { expect(render("## A B\n").headings[0].id).to_eq("a-b"); });

    spec::it("strips markup from the text", [] {
      expect(render("A *bold* word.\n").text).to_eq("A bold word.");
    });

    spec::it("keeps code span content in the text", [] {
      expect(render("Use `run`.\n").text).to_contain("run");
    });

    spec::it("leaves html out of the text", [] {
      expect(render("[link](/u)\n").text).not_to_contain("href");
    });

    spec::context("heading anchors", [] {
      spec::it("are off by default", [] {
        expect(render("# Title\n").html).to_eq("<h1>Title</h1>\n");
      });

      spec::it("add an id and a link when asked for", [] {
        blogin::RenderOptions options;
        options.heading_anchors = true;

        expect(render("# Title\n", options).html)
          .to_eq("<h1 id=\"title\">Title<a class=\"anchor\" href=\"#title\">#</a></h1>\n");
      });
    });

    spec::context("code blocks that are not code", [] {
      spec::it("renders a mermaid block for the browser", [] {
        expect(render("```mermaid\ngraph TD;\n```\n").html).to_contain("<pre class=\"mermaid\">");
      });

      spec::it("renders a math block as display maths", [] {
        expect(render("```math\nx^2\n```\n").html).to_contain("math math-display");
      });
    });
  });

  spec::describe("tables of contents", [] {
    auto headings = spec::let([] {
      return std::vector<blogin::Heading>{
        {1, "One", "one"}, {2, "One A", "one-a"}, {2, "One B", "one-b"}, {1, "Two", "two"},
      };
    });

    spec::it("keeps top-level headings at the root", [=] {
      expect(blogin::toc::build(headings()).size()).to_eq(std::size_t{2});
    });

    spec::it("nests a deeper heading under the one above it", [=] {
      expect(blogin::toc::build(headings())[0].children.size()).to_eq(std::size_t{2});
    });

    spec::it("renders a nested list", [=] {
      expect(blogin::toc::render(blogin::toc::build(headings()))).to_contain("<ul><li><a href=\"#one\">");
    });

    spec::it("renders nothing for no headings", [] { expect(blogin::toc::render({})).to_eq(""); });

    spec::it("escapes a heading title", [] {
      const std::vector<blogin::Heading> unsafe{{1, "a < b", "a-b"}};

      expect(blogin::toc::render(blogin::toc::build(unsafe))).to_contain("a &lt; b");
    });

    spec::it("escapes a quote in a heading title", [] {
      const std::vector<blogin::Heading> quoted{{1, R"(say "hi")", "say-hi"}};

      expect(blogin::toc::render(blogin::toc::build(quoted))).to_contain("say &quot;hi&quot;");
    });

    spec::it("escapes an ampersand in a heading title", [] {
      const std::vector<blogin::Heading> ampersand{{1, "this & that", "this-that"}};

      expect(blogin::toc::render(blogin::toc::build(ampersand))).to_contain("this &amp; that");
    });

    // A deeper heading with nothing above it has no parent to nest under.
    spec::it("keeps a heading that starts deeper than the first level", [] {
      const std::vector<blogin::Heading> deep{{3, "orphan", "orphan"}};

      expect(blogin::toc::build(deep).size()).to_eq(std::size_t{1});
    });
  });

  spec::describe("the content stylesheet", [] {
    spec::it("escapes markup inside highlighted code", [] {
      expect(blogin::highlight::render("a & b < c", "rust")).to_contain("&amp;");
    });

    spec::it("styles highlighting", [] { expect(std::string(blogin::style::content_css())).to_contain(".hl-keyword"); });

    spec::it("styles heading anchors", [] {
      expect(std::string(blogin::style::content_css())).to_contain(".anchor");
    });

    spec::it("styles the dark theme", [] {
      expect(std::string(blogin::style::content_css())).to_contain("[data-theme=\"dark\"]");
    });
  });
}
