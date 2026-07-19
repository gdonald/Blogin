#include "feed.h"

#include <format>

#include "date.h"
#include "json.h"
#include "value.h"

namespace blogin::feed {
namespace {

void escape_xml(std::string& out, std::string_view text) {
  for (const char character : text) {
    switch (character) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default: out += character; break;
    }
  }
}

std::string element(std::string_view name, std::string_view value, std::string_view indent = "") {
  std::string out(indent);

  out += '<';
  out += name;
  out += '>';
  escape_xml(out, value);
  out += "</";
  out += name;
  out += ">\n";

  return out;
}

// Feeds want a timestamp, and a post carries a date. A date with no time on it
// reads as midnight UTC.
std::string atom_time(std::string_view date) {
  return date.empty() ? "1970-01-01T00:00:00Z" : std::string(date) + "T00:00:00Z";
}

std::string rss_time(std::string_view iso) {
  const std::optional<Date> date = Date::parse(iso);

  if (!date) {
    return "Thu, 01 Jan 1970 00:00:00 +0000";
  }

  return date->format("%a, %d %b %Y") + " 00:00:00 +0000";
}

}  // namespace

std::string_view filename_for(std::string_view format) {
  if (format == "rss") {
    return "rss.xml";
  }

  if (format == "json") {
    return "feed.json";
  }

  return "feed.xml";
}

std::string atom(const FeedInfo& info) {
  std::string out = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<feed xmlns=\"http://www.w3.org/2005/Atom\">\n";

  out += element("title", info.title, "  ");
  out += element("id", info.site_url, "  ");

  out += "  <link href=\"";
  escape_xml(out, info.site_url);
  out += "\"/>\n  <link href=\"";
  escape_xml(out, info.feed_url);
  out += "\" rel=\"self\"/>\n";

  out += element("updated", atom_time(info.updated), "  ");

  for (const FeedEntry& entry : info.entries) {
    out += "  <entry>\n";
    out += element("title", entry.title, "    ");
    out += element("id", entry.url, "    ");

    out += "    <link href=\"";
    escape_xml(out, entry.url);
    out += "\"/>\n";

    out += element("updated", atom_time(entry.date), "    ");

    if (!entry.summary.empty()) {
      out += element("summary", entry.summary, "    ");
    }

    out += "  </entry>\n";
  }

  out += "</feed>\n";

  return out;
}

std::string rss(const FeedInfo& info) {
  std::string out = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<rss version=\"2.0\">\n  <channel>\n";

  out += element("title", info.title, "    ");
  out += element("link", info.site_url, "    ");
  out += element("description", info.title, "    ");

  if (!info.updated.empty()) {
    out += element("lastBuildDate", rss_time(info.updated), "    ");
  }

  for (const FeedEntry& entry : info.entries) {
    out += "    <item>\n";
    out += element("title", entry.title, "      ");
    out += element("link", entry.url, "      ");

    out += "      <guid isPermaLink=\"true\">";
    escape_xml(out, entry.url);
    out += "</guid>\n";

    out += element("pubDate", rss_time(entry.date), "      ");
    out += element("description", entry.summary.empty() ? entry.title : entry.summary, "      ");

    out += "    </item>\n";
  }

  out += "  </channel>\n</rss>\n";

  return out;
}

std::string json_feed(const FeedInfo& info) {
  Value items = Value::array();

  for (const FeedEntry& entry : info.entries) {
    Value item = Value::object();
    item.set("id", Value(entry.url));
    item.set("url", Value(entry.url));
    item.set("title", Value(entry.title));
    item.set("date_published", Value(atom_time(entry.date)));
    item.set("content_text", Value(entry.summary.empty() ? entry.title : entry.summary));

    items.push(std::move(item));
  }

  Value document = Value::object();
  document.set("version", Value("https://jsonfeed.org/version/1.1"));
  document.set("title", Value(info.title));
  document.set("home_page_url", Value(info.site_url));
  document.set("feed_url", Value(info.feed_url));
  document.set("items", std::move(items));

  return to_json(document, JsonStyle::pretty);
}

std::string sitemap(const std::vector<std::string>& locations) {
  std::string out =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";

  for (const std::string& location : locations) {
    out += "  <url><loc>";
    escape_xml(out, location);
    out += "</loc></url>\n";
  }

  out += "</urlset>\n";

  return out;
}

std::string robots_txt(std::string_view base_url) {
  std::string out = "User-agent: *\nAllow: /";

  if (!base_url.empty()) {
    std::string_view trimmed = base_url;

    while (trimmed.ends_with('/')) {
      trimmed.remove_suffix(1);
    }

    out += "\nSitemap: ";
    out += trimmed;
    out += "/sitemap.xml";
  }

  return out + "\n";
}

}  // namespace blogin::feed
