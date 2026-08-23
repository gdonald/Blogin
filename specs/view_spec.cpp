#include <string>
#include <vector>

#include "support/spec.h"
#include "view.h"

using blogin::Chrome;
using blogin::ListingView;
using blogin::Post;
using blogin::PostView;
using blogin::Value;
using spec::expect;

namespace {

Chrome sample_chrome() {
  Chrome chrome;

  Value site = Value::object();
  site.set("title", Value("Blogin"));
  site.set("base-url", Value("https://example.com"));
  site.set("author", Value("Greg"));

  chrome.site = site;
  chrome.section = "posts";
  chrome.url = "/posts/hello";

  blogin::NavNode posts;
  posts.name = "posts";
  posts.label = "Writing";
  posts.path = "posts";
  posts.url = "/posts/";

  chrome.nav = {posts};

  return chrome;
}

Post sample_post() {
  return Post::parse("---\ntitle: Hello\ndate: 2024-03-07\n---\nBody.\n", "hello.md").value();
}

PostView sample_page() {
  PostView page;
  page.chrome = sample_chrome();
  page.body_html = "<p>Body.</p>";
  page.summary = "A summary.";
  page.word_count = 2;
  page.reading_time = 1;

  return page;
}

std::string value_of(blogin::ViewContext& context, std::string_view name) {
  const Value* found = context.lookup(name);

  return found == nullptr ? "<missing>" : std::string(found->as_string());
}

}  // namespace

SPEC {
  spec::describe("the view", [] {
    spec::context("chrome", [] {
      auto context = spec::let([] {
        PostView page = sample_page();
        const Post post = sample_post();
        page.post = &post;

        return std::make_shared<blogin::ViewContext>(blogin::view::build(page));
      });

      spec::it("offers the site title", [=] { expect(value_of(*context(), "site-title")).to_eq("Blogin"); });

      spec::it("offers the section", [=] { expect(value_of(*context(), "section")).to_eq("posts"); });

      spec::it("labels a section from its nav entry", [=] {
        expect(value_of(*context(), "section-label")).to_eq("Writing");
      });

      spec::it("labels a section the nav does not know from its own name", [] {
        PostView page = sample_page();
        const Post post = sample_post();
        page.post = &post;
        page.chrome.section = "release-notes";

        blogin::ViewContext built = blogin::view::build(page);

        expect(value_of(built, "section-label")).to_eq("Release Notes");
      });

      // A subsection is labelled by its own name, not by the path it sits at.
      spec::it("labels a subsection from the last part of its path", [] {
        PostView page = sample_page();
        const Post post = sample_post();
        page.post = &post;
        page.chrome.section = "guide/release-notes";

        blogin::ViewContext built = blogin::view::build(page);

        expect(value_of(built, "section-label")).to_eq("Release Notes");
      });

      spec::it("offers the navigation tree", [=] {
        expect(context()->lookup("nav-nodes")->size()).to_eq(std::size_t{1});
      });

      spec::it("marks the current nav item", [=] {
        expect(context()->lookup("nav-nodes")->at(0)["current"].truthy()).to_be_true();
      });
    });

    spec::context("titles", [] {
      spec::it("joins the site and page titles", [] {
        expect(blogin::view::compose_title("Blogin", "Hello")).to_eq("Blogin :: Hello");
      });

      spec::it("does not repeat the site title", [] {
        expect(blogin::view::compose_title("Blogin", "Blogin")).to_eq("Blogin");
      });

      spec::it("uses the site title alone when a page has none", [] {
        expect(blogin::view::compose_title("Blogin", "")).to_eq("Blogin");
      });
    });

    spec::context("head metadata", [] {
      auto meta = spec::let([] {
        return blogin::view::head_meta(sample_chrome(), "Hello", "A summary.", "article");
      });

      // A base url written with a trailing slash must not double the separator.
      spec::it("writes a canonical link when the base url ends in a slash", [] {
        Chrome chrome = sample_chrome();
        chrome.site.set("base-url", Value("https://example.com/"));

        expect(blogin::view::head_meta(chrome, "Hello", "", "article"))
          .to_contain(R"(<link rel="canonical" href="https://example.com/posts/hello"/>)");
      });

      spec::it("writes a canonical link", [=] {
        expect(meta()).to_contain(R"(<link rel="canonical" href="https://example.com/posts/hello"/>)");
      });

      spec::it("writes a description", [=] { expect(meta()).to_contain("name=\"description\""); });

      spec::it("writes open graph tags", [=] { expect(meta()).to_contain("property=\"og:title\""); });

      spec::it("writes twitter tags", [=] { expect(meta()).to_contain("name=\"twitter:card\""); });

      spec::it("escapes what it writes", [] {
        Chrome chrome = sample_chrome();

        expect(blogin::view::head_meta(chrome, "a \" b", "", "article")).to_contain("&quot;");
      });

      spec::it("leaves out a tag it has nothing for", [] {
        expect(blogin::view::head_meta(sample_chrome(), "Hello", "", "article"))
          .not_to_contain("og:description");
      });
    });

    spec::context("a post page", [] {
      auto context = spec::let([] {
        PostView page = sample_page();
        const Post post = sample_post();
        page.post = &post;

        Value tag = Value::object();
        tag.set("name", Value("raku"));
        tag.set("url", Value("/tags/raku"));
        page.tags = Value::array({tag});

        page.headings = {blogin::Heading{2, "A section", "a-section"}};
        page.previous_url = "/posts/older";
        page.previous_title = "Older";

        return std::make_shared<blogin::ViewContext>(blogin::view::build(page));
      });

      spec::it("offers the title", [=] { expect(value_of(*context(), "title")).to_eq("Hello"); });

      spec::it("offers the date", [=] { expect(value_of(*context(), "date")).to_eq("2024-03-07"); });

      spec::it("offers the body", [=] { expect(value_of(*context(), "body")).to_contain("<p>Body.</p>"); });

      spec::it("reports having tags", [=] { expect(context()->lookup("has-tags")->truthy()).to_be_true(); });

      spec::it("builds a table of contents", [=] {
        expect(value_of(*context(), "toc-html")).to_contain("#a-section");
      });

      spec::it("writes post navigation", [=] {
        expect(value_of(*context(), "post-nav-html")).to_contain("/posts/older");
      });

      spec::it("writes no navigation when there is nowhere to go", [] {
        PostView page = sample_page();
        const Post post = sample_post();
        page.post = &post;

        auto built = blogin::view::build(page);

        expect(value_of(built, "post-nav-html")).to_eq("");
      });

      spec::it("falls back to the summary for a description", [=] {
        expect(value_of(*context(), "meta-description")).to_eq("A summary.");
      });
    });

    spec::context("a listing page", [] {
      auto listing = spec::let([] {
        ListingView built;
        built.chrome = sample_chrome();
        built.page_number = 2;
        built.total_pages = 3;
        built.previous_url = "/posts/";
        built.next_url = "/posts/page/3";
        built.page_urls = {"/posts/", "/posts/page/2", "/posts/page/3"};

        return built;
      });

      spec::it("offers the page number", [=] {
        auto context = blogin::view::build(listing());

        expect(context.lookup("page-number")->as_integer()).to_eq(std::int64_t{2});
      });

      spec::it("writes a pagination bar", [=] {
        expect(blogin::view::pagination_html(listing())).to_contain("aria-label=\"Pagination\"");
      });

      spec::it("marks the current page", [=] {
        expect(blogin::view::pagination_html(listing())).to_contain("aria-current=\"page\"");
      });

      spec::it("offers pagination links for a layout that writes its own", [=] {
        auto context = blogin::view::build(listing());

        expect(context.lookup("pagination-links")->size()).to_eq(std::size_t{3});
      });

      spec::it("writes no bar for a single page", [] {
        ListingView single;
        single.chrome = sample_chrome();
        single.page_urls = {"/posts/"};

        expect(blogin::view::pagination_html(single)).to_eq("");
      });

      // First, last, previous, and next are separate controls, so the numbered
      // part never grows past seven.
      spec::it("shows at most seven numbers", [] {
        expect(blogin::view::pagination_window(10, 100).size()).to_eq(std::size_t{7});
      });

      spec::it("clamps the window at the start", [] {
        expect(blogin::view::pagination_window(1, 100)[0]).to_eq(1);
      });

      spec::it("clamps the window at the end", [] {
        expect(blogin::view::pagination_window(100, 100).back()).to_eq(100);
      });
    });

    // Every helper a layout can call. Each is given nothing as well as
    // something, since a template calling one with no arguments should get an
    // empty answer, never a crash.
    spec::context("the functions a layout can call", [] {
      auto context = spec::let([] { return blogin::view::build(sample_page()); });

      const auto call = [](const blogin::ViewContext& built, std::string_view name,
                           const std::vector<blogin::ViewContext::Argument>& arguments) {
        return (*built.function(name))(arguments);
      };

      spec::it("looks a framework class up", [=] {
        expect(std::string(call(context(), "framework-class", {{{}, Value("nav")}}).as_string()))
          .to_eq("");
      });

      spec::it("gives no framework class when asked for none", [=] {
        expect(std::string(call(context(), "framework-class", {}).as_string())).to_eq("");
      });

      spec::it("marks the nav item being looked at", [=] {
        Value node = Value::object();
        node.set("current", Value(true));

        expect(call(context(), "nav-current", {{{}, node}}).truthy()).to_be_true();
      });

      spec::it("marks nothing when given no nav item", [=] {
        expect(call(context(), "nav-current", {}).truthy()).to_be_false();
      });

      spec::it("formats a date to the pattern it was given", [=] {
        expect(std::string(
                 call(context(), "format-date", {{{}, Value("2024-03-07")}, {{}, Value("%B %e, %Y")}})
                   .as_string()))
          .to_contain("March");
      });

      spec::it("formats a date the usual way when given no pattern", [=] {
        expect(std::string(call(context(), "format-date", {{{}, Value("2024-03-07")}}).as_string()))
          .to_eq("2024-03-07");
      });

      spec::it("formats nothing when given no date", [=] {
        expect(std::string(call(context(), "format-date", {}).as_string())).to_eq("");
      });

      spec::it("truncates to the length it was given", [=] {
        expect(std::string(
                 call(context(), "truncate", {{{}, Value("one two three")}, {{}, Value(7)}}).as_string()))
          .to_contain("one");
      });

      spec::it("truncates to a default length when given none", [=] {
        expect(std::string(call(context(), "truncate", {{{}, Value("short")}}).as_string()))
          .to_eq("short");
      });

      spec::it("truncates nothing when given nothing", [=] {
        expect(std::string(call(context(), "truncate", {}).as_string())).to_eq("");
      });

      spec::it("groups entries by a key", [=] {
        Value first = Value::object();
        first.set("date", Value("2024-03-07"));

        Value second = Value::object();
        second.set("date", Value("2024-03-07"));

        const Value grouped =
          call(context(), "group-by", {{{}, Value::array({first, second})}, {{}, Value("date")}});

        expect(grouped.at(0)["items"].size()).to_eq(std::size_t{2});
      });

      spec::it("keys each group", [=] {
        Value entry = Value::object();
        entry.set("date", Value("2024-03-07"));

        const Value grouped =
          call(context(), "group-by", {{{}, Value::array({entry})}, {{}, Value("date")}});

        expect(std::string(grouped.at(0)["key"].as_string())).to_eq("2024-03-07");
      });

      spec::it("groups nothing when told only what to group", [=] {
        expect(call(context(), "group-by", {{{}, Value::array()}}).size()).to_eq(std::size_t{0});
      });

    });

    spec::context("debug markers", [] {
      spec::it("writes nothing when debugging is off", [] {
        PostView page = sample_page();
        auto context = blogin::view::build(page);

        expect(std::string((*context.function("debug-open"))({{{}, Value("partial: header")}}).as_string()))
          .to_eq("");
      });

      spec::it("names the region when debugging is on", [] {
        PostView page = sample_page();
        page.chrome.debug = true;

        auto context = blogin::view::build(page);

        expect(std::string((*context.function("debug-open"))({{{}, Value("partial: header")}}).as_string()))
          .to_contain("begin partial: header");
      });

      spec::it("closes the region when debugging is on", [] {
        PostView page = sample_page();
        page.chrome.debug = true;

        auto context = blogin::view::build(page);

        expect(std::string((*context.function("debug-close"))({{{}, Value("partial: header")}}).as_string()))
          .to_contain("end partial: header");
      });

      // Interpolated text must not be able to end the comment early.
      spec::it("neutralises a double hyphen", [] {
        PostView page = sample_page();
        page.chrome.debug = true;

        auto context = blogin::view::build(page);

        expect(std::string((*context.function("debug-open"))({{{}, Value("a -- b")}}).as_string()))
          .not_to_contain("a -- b");
      });
    });
  });
}
