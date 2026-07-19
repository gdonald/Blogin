#include <atomic>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haml.h"
#include "support/spec.h"
#include "template_store.h"
#include "view_context.h"

using blogin::Value;
using blogin::ViewContext;
using spec::expect;

namespace {

ViewContext sample_context() {
  ViewContext context;

  context.set("title", Value("Blogin"));
  context.set("unsafe", Value("a < b"));
  context.set("markup", Value("<em>hi</em>"));
  context.set("draft", Value(false));
  context.set("published", Value(true));
  context.set("shown", Value(true));
  context.set("hidden", Value(false));

  Value tag = Value::object();
  tag.set("name", Value("raku"));
  tag.set("url", Value("/tags/raku"));

  context.set("tags", Value::array({tag}));
  context.set("empty", Value::array());
  context.set("nothing", Value());

  Value locals = Value::object();
  locals.set("who", Value("someone"));
  context.set("here", locals);

  Value child = Value::object();
  child.set("name", Value("child"));
  child.set("children", Value::array());

  Value parent = Value::object();
  parent.set("name", Value("parent"));
  parent.set("children", Value::array({child}));

  context.set("tree", Value::array({parent}));

  return context;
}

std::string render(std::string source, const blogin::haml::RenderOptions& options = {}) {
  const auto compiled = blogin::haml::Template::compile(std::move(source));

  if (!compiled) {
    return "COMPILE ERROR: " + compiled.error().message;
  }

  ViewContext context = sample_context();
  const auto out = blogin::haml::render(*compiled, context, options);

  return out ? *out : "ERROR: " + out.error().message;
}

}  // namespace

SPEC {
  spec::describe("the template engine", [] {
    spec::context("elements", [] {
      spec::it("writes a tag", [] { expect(render("%p")).to_eq("<p></p>\n"); });

      spec::it("writes text inside a tag", [] { expect(render("%p hello")).to_eq("<p>hello</p>\n"); });

      spec::it("nests by indentation", [] {
        expect(render("%div\n  %p hi")).to_eq("<div>\n<p>hi</p>\n</div>\n");
      });

      spec::it("defaults an element with only a class to a div", [] {
        expect(render(".card")).to_eq("<div class=\"card\"></div>\n");
      });

      spec::it("reads an id", [] { expect(render("#main")).to_eq("<div id=\"main\"></div>\n"); });

      spec::it("combines several classes", [] {
        expect(render("%p.one.two")).to_eq("<p class=\"one two\"></p>\n");
      });

      spec::it("combines a tag, an id, and a class", [] {
        expect(render("%section#main.wide")).to_eq("<section id=\"main\" class=\"wide\"></section>\n");
      });

      spec::it("closes a void tag by itself", [] { expect(render("%br")).to_eq("<br />\n"); });

      spec::it("closes an element marked self-closing", [] { expect(render("%foo/")).to_eq("<foo />\n"); });
    });

    spec::context("attributes", [] {
      spec::it("reads the ruby hash form", [] {
        expect(render("%a{href: '/x'}")).to_eq("<a href=\"/x\"></a>\n");
      });

      spec::it("reads the rocket form", [] {
        expect(render("%a{'data-x' => '1'}")).to_eq("<a data-x=\"1\"></a>\n");
      });

      spec::it("reads the html form", [] { expect(render("%a(href='/x')")).to_eq("<a href=\"/x\"></a>\n"); });

      spec::it("reads several attributes", [] {
        expect(render("%a{href: '/x', rel: 'me'}")).to_contain("rel=\"me\"");
      });

      spec::it("interpolates into a value", [] {
        expect(render("%a{href: \"#{title}\"}")).to_eq("<a href=\"Blogin\"></a>\n");
      });

      spec::it("evaluates an unquoted value", [] {
        expect(render("%a{href: title}")).to_eq("<a href=\"Blogin\"></a>\n");
      });

      spec::it("merges an attribute class with a shorthand class", [] {
        expect(render("%p.one{class: 'two'}")).to_contain("class=\"one two\"");
      });

      spec::it("escapes a value", [] {
        expect(render("%a{title: unsafe}")).to_contain("title=\"a &lt; b\"");
      });

      // {hidden: shown} should read the way it looks.
      spec::it("writes a true attribute bare", [] {
        expect(render("%input{disabled: shown}")).to_contain(" disabled");
      });

      spec::it("leaves a false attribute off entirely", [] {
        expect(render("%input{disabled: hidden}")).not_to_contain("disabled");
      });

      spec::it("leaves a null attribute off entirely", [] {
        expect(render("%input{value: nothing}")).not_to_contain("value");
      });
    });

    spec::context("output", [] {
      spec::it("escapes what it writes", [] { expect(render("= unsafe")).to_eq("a &lt; b\n"); });

      spec::it("writes raw output unescaped", [] { expect(render("!= markup")).to_eq("<em>hi</em>\n"); });

      spec::it("writes output inside a tag", [] { expect(render("%h1= title")).to_eq("<h1>Blogin</h1>\n"); });

      spec::it("writes raw output inside a tag", [] {
        expect(render("%div!= markup")).to_eq("<div><em>hi</em></div>\n");
      });

      spec::it("interpolates in plain text", [] {
        expect(render("Hello #{title}")).to_eq("Hello Blogin\n");
      });

      // There is no way to write markup accidentally.
      spec::it("escapes an interpolated value", [] {
        expect(render("Value: #{unsafe}")).to_contain("a &lt; b");
      });
    });

    spec::context("control flow", [] {
      spec::it("renders a true branch", [] { expect(render("- if published\n  %p yes")).to_contain("yes"); });

      spec::it("skips a false branch", [] {
        expect(render("- if draft\n  %p no")).not_to_contain("no");
      });

      spec::it("takes the else branch", [] {
        expect(render("- if draft\n  %p no\n- else\n  %p yes")).to_contain("yes");
      });

      spec::it("writes only one branch", [] {
        expect(render("- if published\n  %p yes\n- else\n  %p no")).not_to_contain("no");
      });

      spec::it("takes an elsif branch", [] {
        expect(render("- if draft\n  %p a\n- elsif published\n  %p b\n- else\n  %p c")).to_contain("b");
      });

      spec::it("inverts with unless", [] { expect(render("- unless draft\n  %p yes")).to_contain("yes"); });

      spec::it("treats an empty list as false", [] {
        expect(render("- if empty\n  %p no")).not_to_contain("no");
      });

      spec::it("loops over a list", [] {
        expect(render("- for tags -> $tag\n  %li= $tag<name>")).to_eq("<li>raku</li>\n");
      });

      spec::it("reads the loop variable with either access form", [] {
        expect(render("- for tags -> $tag\n  %a{href: $tag.url}= $tag.name")).to_contain("/tags/raku");
      });

      spec::it("renders nothing for an empty list", [] {
        expect(render("- for empty -> $x\n  %p item")).to_eq("");
      });

      // Iterating a scalar is a mistake rather than an empty result.
      spec::it("refuses to iterate something that is not a list", [] {
        expect(render("- for title -> $x\n  %p item")).to_contain("cannot iterate");
      });

      spec::it("starts a new chain at a second if", [] {
        expect(render("- if published\n  %p a\n- if published\n  %p b")).to_contain("<p>b</p>");
      });

      spec::it("ends a chain at a line that is not a branch", [] {
        expect(render("- if published\n  %p a\n%p after")).to_contain("<p>after</p>");
      });

      spec::it("ends a chain at a loop", [] {
        expect(render("- if published\n  %p a\n- for tags -> $tag\n  %p= $tag<name>"))
          .to_contain("raku");
      });

      spec::it("reports a failure inside a branch it took", [] {
        expect(render("- if published\n  %p= nonesuch")).to_contain("no such name");
      });

      spec::it("refuses an else with no if above it", [] {
        expect(render("- else\n  %p x")).to_contain("need an 'if' above them");
      });

      spec::it("refuses a keyword it does not know", [] {
        expect(render("- while true\n  %p x")).to_contain("not something a template can do");
      });
    });

    spec::context("attributes written as expressions", [] {
      spec::it("takes a class from an expression", [] {
        expect(render("%p{class: title}")).to_contain("class=\"Blogin\"");
      });

      spec::it("takes an id from an expression", [] {
        expect(render("%p{id: title}")).to_contain("id=\"Blogin\"");
      });

      spec::it("merges an expression class with a shorthand one", [] {
        expect(render("%p.one{class: title}")).to_contain("class=\"one Blogin\"");
      });
    });

    spec::context("doctype and comments", [] {
      spec::it("writes a html5 doctype", [] { expect(render("!!! 5")).to_eq("<!DOCTYPE html>\n"); });

      // A template comment is for whoever reads the template.
      spec::it("leaves a comment out of the page", [] { expect(render("-# a note")).to_eq(""); });

      // Only a line of its own is a comment. A footer writes `.copy // 2026`
      // and means the slashes.
      spec::it("keeps a slash that follows a tag on the same line", [] {
        expect(render("%p // documentation")).to_eq("<p>// documentation</p>\n");
      });
    });

    // What a template writes to say a line is text and nothing else.
    spec::context("escaped text", [] {
      spec::it("drops the backslash and keeps the rest of the line", [] {
        expect(render("\\= not an expression")).to_eq("= not an expression\n");
      });

      spec::it("keeps a space the backslash was protecting", [] {
        expect(render("%span\n  \\ behave.dev")).to_contain("> behave.dev<");
      });
    });

    spec::context("filters", [] {
      spec::it("writes plain content through", [] {
        expect(render(":plain\n  <svg></svg>")).to_eq("<svg></svg>\n");
      });

      spec::it("escapes content it is told to", [] {
        expect(render(":escaped\n  <b>")).to_contain("&lt;b&gt;");
      });

      spec::it("wraps javascript in a script tag", [] {
        expect(render(":javascript\n  var x = 1;")).to_contain("<script>");
      });

      spec::it("wraps css in a style tag", [] { expect(render(":css\n  a {}")).to_contain("<style>"); });

      spec::it("refuses a filter it does not know", [] {
        expect(render(":markdown\n  # hi")).to_contain("no such filter");
      });
    });

    spec::context("yield", [] {
      spec::it("writes the body it was given", [] {
        blogin::haml::RenderOptions options;
        options.body = "<p>page</p>";

        expect(render("%main\n  = yield", options)).to_contain("<p>page</p>");
      });
    });

    spec::context("partials", [] {
      auto store = spec::let([] {
        // Its own directory per example, so examples running side by side do
        // not delete each other's fixtures.
        static std::atomic<int> counter{0};

        const std::filesystem::path root = std::filesystem::temp_directory_path() / "blogin-haml-specs" /
                                           std::to_string(counter.fetch_add(1));

        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);

        const auto write = [&](std::string_view name, std::string_view body) {
          std::ofstream out(root / name, std::ios::binary | std::ios::trunc);
          out.write(body.data(), static_cast<std::streamsize>(body.size()));
        };

        write("_entry.haml", "%li= $entry<name>\n");
        write("_greeting.haml", "%p= title\n");
        write("_local.haml", "%p= $who\n");
        write("_node.haml", "%li\n  = $node<name>\n  - if $node<children>\n    != render(:partial<node>, :collection($node<children>), :as<node>)\n");

        return std::make_shared<blogin::TemplateStore>(
          blogin::TemplateStore::load({root}).value());
      });

      auto options = spec::let([=] {
        blogin::haml::RenderOptions built;
        built.partial = [store = store()](std::string_view name) { return store->find_partial(name); };

        return built;
      });

      spec::it("renders a partial", [=] {
        expect(render("!= render(:partial<greeting>)", options())).to_eq("<p>Blogin</p>\n\n");
      });

      spec::it("renders a partial once per item of a collection", [=] {
        expect(render("!= render(:partial<entry>, :collection(tags), :as<entry>)", options()))
          .to_contain("<li>raku</li>");
      });

      spec::it("passes locals through", [=] {
        expect(render("!= render(:partial<local>, :locals(here))", options())).to_contain("someone");
      });

      // The navigation menu is a partial that renders itself.
      spec::it("lets a partial render itself", [=] {
        expect(render("!= render(:partial<node>, :collection(tree), :as<node>)", options()))
          .to_contain("child");
      });

      spec::it("refuses a partial that is not there", [=] {
        expect(render("!= render(:partial<absent>)", options())).to_contain("no such partial");
      });

      spec::it("refuses a render with no partial named", [=] {
        expect(render("!= render()", options())).to_contain("needs a :partial");
      });

      spec::it("reports a bad name in a partial's collection", [=] {
        expect(render("!= render(:partial<entry>, :collection(nonesuch), :as<entry>)", options()))
          .to_contain("no such name");
      });

      spec::it("reports a bad name in a partial's locals", [=] {
        expect(render("!= render(:partial<local>, :locals(nonesuch))", options()))
          .to_contain("no such name");
      });

      // A partial expecting a loop variable, rendered without one.
      spec::it("reports a failure inside the partial itself", [=] {
        expect(render("!= render(:partial<entry>)", options())).to_contain("no such local");
      });
    });

    spec::context("fragment reuse", [] {
      auto cache = spec::let([] { return std::make_shared<blogin::FragmentCache>(); });

      auto options = spec::let([=] {
        blogin::haml::RenderOptions built;
        built.fragments = cache().get();

        return built;
      });

      // A page is one render of one compiled template, so reuse across pages is
      // reuse across renders of the same template rather than of two that
      // happen to say the same thing.
      const auto render_pages = [](const std::string& source,
                                   const blogin::haml::RenderOptions& render_options,
                                   std::initializer_list<std::string> titles) {
        std::vector<std::string> out;

        const auto compiled = blogin::haml::Template::compile(source);

        for (const std::string& title : titles) {
          ViewContext context = sample_context();
          context.set("title", Value(title));

          out.push_back(blogin::haml::render(*compiled, context, render_options).value_or("RENDER ERROR"));
        }

        return out;
      };

      spec::it("renders a fragment", [=] {
        expect(render("!= cache-fragment('a', { title })", options())).to_contain("Blogin");
      });

      // The key comes from what the fragment read, so two pages reading the
      // same values share it without anyone declaring that they may.
      spec::it("reuses a fragment across pages that read the same values", [=] {
        render_pages("!= cache-fragment('a', { title })", options(), {"Blogin", "Blogin"});

        expect(cache()->hits()).to_be_greater_than(std::size_t{0});
      });

      spec::it("keeps one entry when the reads agree", [=] {
        render_pages("!= cache-fragment('a', { title })", options(), {"Blogin", "Blogin"});

        expect(cache()->size()).to_eq(std::size_t{1});
      });

      // Reuse that still renders the fragment has saved nothing. The second
      // page works its key out from what the first one read.
      spec::it("renders a reused fragment once rather than once a page", [=] {
        render_pages("!= cache-fragment('a', { title })", options(),
                     {"Blogin", "Blogin", "Blogin", "Blogin"});

        expect(cache()->renders()).to_eq(std::size_t{1});
      });

      spec::it("renders a fragment again for a page it reads differently on", [=] {
        render_pages("!= cache-fragment('a', { title })", options(), {"Blogin", "Something else"});

        expect(cache()->renders()).to_eq(std::size_t{2});
      });

      spec::it("gives each page reading differently its own output", [=] {
        const auto pages =
          render_pages("!= cache-fragment('a', { title })", options(), {"Blogin", "Something else"});

        expect(pages.at(1)).to_contain("Something else");
      });

      // Two fragments are two fragments even when they read the same values,
      // and serving one of them in the other's place is the failure this cache
      // exists to be incapable of.
      spec::it("never serves one fragment in another's place", [=] {
        const auto pages = render_pages(
          "!= cache-fragment('header', { 'the header' })\n!= cache-fragment('footer', { 'the footer' })\n",
          options(), {"Blogin", "Blogin"});

        expect(pages.at(1)).to_contain("the footer");
      });
    });

    spec::context("errors", [] {
      spec::it("names what a template asked for that does not exist", [] {
        expect(render("%p= nonesuch")).to_contain("no such name");
      });

      spec::it("suggests a name close to the one written", [] {
        expect(render("%p= titel")).to_contain("did you mean 'title'");
      });

      // Every place a malformed expression can sit in a template. The parser
      // has a way back out at each of them.
      const std::vector<std::pair<std::string, std::string>> unparseable{
        {"= title +", "expected a value"},
        {"!= title +", "expected a value"},
        {"%p= title +", "expected a value"},
        {"text #{title +}", "expected a value"},
        {"- if title +\n  %p x", "expected a value"},
        {"- for title + -> $x\n  %p x", "expected a value"},
        {"%a{href: title +}", "expected a value"},
        {"%a{href: \"#{title +}\"}", "expected a value"},
        {"%a{'href' => title +}", "expected a value"},
        {"- for tags\n  %p x", "a for loop needs"},
        {"- for tags ->\n  %p x", "a name after '->'"},
        {"%p.", "expected a name after '.'"},
        {"%a{: '/x'}", "expected an attribute name"},
      };

      for (const auto& example : unparseable) {
        spec::it("refuses " + example.first, [example] {
          expect(render(example.first)).to_contain(example.second);
        });
      }

      spec::it("reads a bare attribute written on its own", [] {
        expect(render("%input{disabled}")).to_contain("disabled");
      });

      spec::it("reads an interpolation holding a brace of its own", [] {
        expect(render("text #{ {brand: title}<brand> }")).to_contain("Blogin");
      });

      spec::it("refuses an unterminated attribute list", [] {
        expect(render("%a{href: '/x'")).to_contain("unterminated attribute list");
      });

      spec::it("refuses an unterminated interpolation", [] {
        expect(render("text #{title")).to_contain("unterminated");
      });

      // A name that does not exist is the same failure wherever a template
      // writes it, and each place has its own way back out to the caller.
      const std::vector<std::pair<std::string, std::string>> failing{
        {"= nonesuch", "escaped output"},
        {"!= nonesuch", "raw output"},
        {"plain #{nonesuch}", "text"},
        {"%a{href: nonesuch}", "an attribute value"},
        {"%a{href: \"#{nonesuch}\"}", "an interpolated attribute"},
        {"%p= nonesuch", "a tag's own output"},
        {"- if nonesuch\n  %p yes", "an if condition"},
        {"- unless nonesuch\n  %p yes", "an unless condition"},
        {"- if draft\n  %p no\n- elsif nonesuch\n  %p maybe", "an elsif condition"},
        {"- for nonesuch -> $item\n  %p x", "a loop's collection"},
        {"- for tags -> $tag\n  %p= nonesuch", "a loop body"},
        {"%div\n  %p= nonesuch", "a nested element"},
        {"!= cache-fragment('key', { nonesuch })", "a cached fragment"},
      };

      for (const auto& example : failing) {
        spec::it("reports a bad name in " + example.second, [example] {
          expect(render(example.first)).to_contain("no such name");
        });
      }

      spec::it("reports a bad name in a class attribute", [] {
        expect(render("%p{class: nonesuch}")).to_contain("no such name");
      });

      spec::it("reports a bad name in an id attribute", [] {
        expect(render("%p{id: nonesuch}")).to_contain("no such name");
      });

      spec::it("refuses a filter it does not have", [] {
        expect(render(":markdown\n  # hi")).to_contain("no such filter ':markdown'");
      });

      spec::it("refuses a render with no partial named", [] {
        expect(render("!= render(:collection(tags))")).to_contain("render needs a :partial");
      });
    });
  });
}
