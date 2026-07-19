#include <string>

#include "feed.h"
#include "support/spec.h"

using blogin::FeedEntry;
using blogin::FeedInfo;
using spec::expect;

namespace {

FeedInfo sample() {
  FeedInfo info;
  info.title = "Blogin";
  info.site_url = "https://example.com/";
  info.feed_url = "https://example.com/feed.xml";
  info.updated = "2024-03-07";
  info.entries = {
    FeedEntry{"First & Best", "https://example.com/first", "2024-03-07", "A summary."},
    FeedEntry{"Second", "https://example.com/second", "2024-01-02", ""},
  };

  return info;
}

}  // namespace

SPEC {
  spec::describe("feeds", [] {
    spec::context("atom", [] {
      auto xml = spec::let([] { return blogin::feed::atom(sample()); });

      spec::it("declares itself", [=] { expect(xml()).to_start_with("<?xml version=\"1.0\""); });

      spec::it("uses the atom namespace", [=] { expect(xml()).to_contain("www.w3.org/2005/Atom"); });

      spec::it("carries the site title", [=] { expect(xml()).to_contain("<title>Blogin</title>"); });

      spec::it("carries each entry", [=] { expect(xml()).to_contain("<title>First &amp; Best</title>"); });

      spec::it("escapes an ampersand", [=] { expect(xml()).not_to_contain("First & Best"); });

      // A feed wants a timestamp and a post carries a date, so a date with no
      // time on it reads as midnight UTC.
      spec::it("turns a date into a timestamp", [=] { expect(xml()).to_contain("2024-03-07T00:00:00Z"); });

      spec::it("includes a summary when there is one", [=] { expect(xml()).to_contain("<summary>A summary."); });

      spec::it("leaves the summary out when there is none", [=] {
        expect(xml()).not_to_contain("<summary></summary>");
      });

      spec::it("closes the feed", [=] { expect(xml()).to_contain("</feed>"); });
    });

    spec::context("rss", [] {
      auto xml = spec::let([] { return blogin::feed::rss(sample()); });

      spec::it("declares the version", [=] { expect(xml()).to_contain("<rss version=\"2.0\">"); });

      spec::it("writes a channel", [=] { expect(xml()).to_contain("<channel>"); });

      spec::it("formats a publication date the way rss wants", [=] {
        expect(xml()).to_contain("Thu, 07 Mar 2024 00:00:00 +0000");
      });

      spec::it("marks a guid as a permanent link", [=] { expect(xml()).to_contain("isPermaLink=\"true\""); });

      spec::it("falls back to the title when an entry has no summary", [=] {
        expect(xml()).to_contain("<description>Second</description>");
      });
    });

    spec::context("json feed", [] {
      auto json = spec::let([] { return blogin::feed::json_feed(sample()); });

      spec::it("declares the version", [=] { expect(json()).to_contain("jsonfeed.org/version/1.1"); });

      spec::it("carries the entries", [=] { expect(json()).to_contain(R"("title": "Second")"); });

      spec::it("carries the feed url", [=] { expect(json()).to_contain("feed_url"); });
    });

    spec::context("sitemap", [] {
      auto xml = spec::let([] {
        return blogin::feed::sitemap({"https://example.com/", "https://example.com/a?b=1"});
      });

      spec::it("uses the sitemap namespace", [=] { expect(xml()).to_contain("sitemaps.org/schemas/sitemap"); });

      spec::it("writes a location per url", [=] { expect(xml()).to_contain("<loc>https://example.com/</loc>"); });

      spec::it("escapes a url", [=] { expect(xml()).to_contain("a?b=1"); });

      spec::it("writes an empty urlset for no urls", [] {
        expect(blogin::feed::sitemap({})).to_contain("</urlset>");
      });
    });

    spec::context("robots", [] {
      spec::it("allows everything", [] { expect(blogin::feed::robots_txt("")).to_contain("Allow: /"); });

      spec::it("points at the sitemap when a base url is known", [] {
        expect(blogin::feed::robots_txt("https://example.com")).to_contain("Sitemap: https://example.com/sitemap.xml");
      });

      spec::it("does not double a trailing slash", [] {
        expect(blogin::feed::robots_txt("https://example.com/")).to_contain("com/sitemap.xml");
      });

      spec::it("leaves the sitemap out when there is no base url", [] {
        expect(blogin::feed::robots_txt("")).not_to_contain("Sitemap");
      });
    });

    spec::context("filenames", [] {
      spec::it("names the atom feed", [] {
        expect(std::string(blogin::feed::filename_for("atom"))).to_eq("feed.xml");
      });

      spec::it("names the rss feed", [] {
        expect(std::string(blogin::feed::filename_for("rss"))).to_eq("rss.xml");
      });

      spec::it("names the json feed", [] {
        expect(std::string(blogin::feed::filename_for("json"))).to_eq("feed.json");
      });
    });
  });
}
