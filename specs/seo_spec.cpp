#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>

#include "config.h"
#include "site.h"
#include "support/spec.h"

using spec::expect;

namespace {

void write(const std::filesystem::path& path, std::string_view body) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

// Three posts in one section, one to a listing page, so the listing runs to
// three pages. `config` is spliced into blogin.json.
std::filesystem::path make_site(std::string_view config = {}) {
  const std::filesystem::path root = spec::scratch_directory("seo");

  write(root / "blogin.json",
        std::string(R"({"title":"Indexed","base-url":"https://example.com","home-section":"posts",)") +
          R"("page-size":1,"search":false,"feed-formats":["atom","json"])" + std::string(config) + "}");

  write(root / "layouts" / "base.haml", "!!! 5\n%html\n  %head\n    != head-meta\n  %body\n    != yield\n");
  write(root / "layouts" / "show.haml", "%article\n  %h1= title\n  != body\n");
  write(root / "layouts" / "index.haml", "%section\n  %h1= heading\n");

  write(root / "content" / "posts" / "first.md",
        "---\ntitle: First\ndate: 2024-03-07\nupdated: 2024-04-01\n---\nThe first post.\n");
  write(root / "content" / "posts" / "second.md", "---\ntitle: Second\ndate: 2024-03-08\n---\nThe second post.\n");
  write(root / "content" / "posts" / "hidden.md",
        "---\ntitle: Hidden\ndate: 2024-03-09\nnoindex: true\n---\nA page to leave out.\n");

  return root;
}

blogin::BuildOptions options_for(const std::filesystem::path& root) {
  const auto config = blogin::Config::load(root / "blogin.json").value();

  return blogin::BuildOptions::around(root / "content", config);
}

blogin::BuildOptions built(std::string_view config = {}) {
  const blogin::BuildOptions options = options_for(make_site(config));

  blogin::build(options);

  return options;
}

}  // namespace

SPEC {
  spec::describe("search engine metadata in a built site", [] {
    spec::context("a post marked noindex", [] {
      auto options = spec::let([] { return built(); });

      spec::it("asks search engines to leave it out", [=] {
        expect(read(options().output / "posts" / "hidden" / "index.html"))
          .to_contain(R"(<meta name="robots" content="noindex"/>)");
      });

      spec::it("is left out of the sitemap", [=] {
        expect(read(options().output / "sitemap.xml")).not_to_contain("/posts/hidden/");
      });

      spec::it("leaves the other posts indexable", [=] {
        expect(read(options().output / "posts" / "second" / "index.html")).not_to_contain("name=\"robots\"");
      });
    });

    spec::context("the sitemap", [] {
      auto sitemap = spec::let([] { return read(built().output / "sitemap.xml"); });

      spec::it("dates a post by its updated date", [=] {
        expect(sitemap()).to_contain("<loc>https://example.com/posts/first/</loc><lastmod>2024-04-01</lastmod>");
      });

      spec::it("dates a post with no updated date by its date", [=] {
        expect(sitemap()).to_contain("<loc>https://example.com/posts/second/</loc><lastmod>2024-03-08</lastmod>");
      });

      spec::it("lists the first page of a listing", [=] {
        expect(sitemap()).to_contain("<loc>https://example.com/</loc></url>");
      });

      spec::it("leaves out the later pages of a listing", [=] { expect(sitemap()).not_to_contain("/page/2"); });
    });

    spec::context("listing pages", [] {
      auto options = spec::let([] { return built(); });

      spec::it("leaves the first page indexable", [=] {
        expect(read(options().output / "index.html")).not_to_contain("name=\"robots\"");
      });

      spec::it("asks search engines to leave out a later page", [=] {
        expect(read(options().output / "page" / "2" / "index.html"))
          .to_contain(R"(<meta name="robots" content="noindex"/>)");
      });
    });

    spec::context("the home page", [] {
      auto options = spec::let([] { return built(); });

      spec::it("carries the WebSite structured data", [=] {
        expect(read(options().output / "index.html")).to_contain(R"("@type":"WebSite","name":"Indexed")");
      });

      spec::it("is the only page that does", [=] {
        expect(read(options().output / "page" / "2" / "index.html")).not_to_contain("WebSite");
      });

    });

    spec::context("a post at the site root as the home page", [] {
      spec::it("carries the WebSite structured data", [] {
        const std::filesystem::path root = make_site(R"(,"home-section":"")");
        write(root / "content" / "index.md", "---\ntitle: Welcome\n---\nThe front page.\n");

        const blogin::BuildOptions options = options_for(root);
        blogin::build(options);

        expect(read(options.output / "index.html")).to_contain(R"("@type":"WebSite")");
      });
    });

    spec::context("a post page", [] {
      auto page = spec::let([] {
        return read(built(R"(,"twitter":"@indexed","author":"Pat")").output / "posts" / "first" / "index.html");
      });

      spec::it("carries the BlogPosting structured data", [=] {
        expect(page()).to_contain(R"("@type":"BlogPosting","headline":"First")");
      });

      spec::it("writes the modified time", [=] { expect(page()).to_contain(R"(content="2024-04-01")"); });

      spec::it("writes the twitter handle", [=] {
        expect(page()).to_contain(R"(<meta name="twitter:site" content="@indexed"/>)");
      });

      spec::it("links each configured feed", [=] {
        spec::aggregate_failures([&] {
          expect(page()).to_contain(
            R"(type="application/atom+xml" title="Indexed" href="https://example.com/feed.xml")");
          expect(page()).to_contain(
            R"(type="application/feed+json" title="Indexed" href="https://example.com/feed.json")");
        });
      });
    });

    spec::context("a site with no posts", [] {
      spec::it("links no feed, since none is written", [] {
        const std::filesystem::path root = make_site();
        std::filesystem::remove_all(root / "content" / "posts");
        std::filesystem::create_directories(root / "content" / "posts");

        const blogin::BuildOptions options = options_for(root);
        blogin::build(options);

        expect(read(options.output / "index.html")).not_to_contain("rel=\"alternate\"");
      });
    });

    spec::context("the 404 page", [] {
      spec::it("asks search engines to leave out the built-in one", [] {
        expect(read(built().output / "404.html")).to_contain(R"(<meta name="robots" content="noindex">)");
      });

      spec::it("asks search engines to leave out one from a 404 layout", [] {
        const std::filesystem::path root = make_site();
        write(root / "layouts" / "404.haml", "%p Not here.\n");

        const blogin::BuildOptions options = options_for(root);
        blogin::build(options);

        expect(read(options.output / "404.html")).to_contain(R"(<meta name="robots" content="noindex"/>)");
      });
    });

    spec::context("rebuilding after another post changes", [] {
      auto sitemap = spec::let([] {
        const std::filesystem::path root = make_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);
        write(root / "content" / "posts" / "second.md",
              "---\ntitle: Second\ndate: 2024-03-08\n---\nThe second post, edited.\n");
        blogin::build(options);

        return read(options.output / "sitemap.xml");
      });

      // The unchanged posts are described from the last build's state, not
      // read again, so what the sitemap says about them comes from there.
      spec::it("keeps a noindex post out of the sitemap", [=] { expect(sitemap()).not_to_contain("/posts/hidden/"); });

      spec::it("keeps an unchanged post's updated date", [=] {
        expect(sitemap()).to_contain("<loc>https://example.com/posts/first/</loc><lastmod>2024-04-01</lastmod>");
      });
    });
  });
}
