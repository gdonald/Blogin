#include "view.h"

#include <algorithm>
#include <format>
#include <utility>

#include "filters.h"
#include "html.h"
#include "json.h"
#include "metrics.h"
#include "slug.h"

namespace blogin::view {
namespace {

void escape_attribute(std::string& out, std::string_view value) {
  append_escaped(out, value);
}

// Neutralises any run of two hyphens, so interpolated text cannot end an HTML
// comment early.
std::string safe_comment(std::string_view text) {
  std::string out;

  std::size_t index = 0;

  while (index < text.size()) {
    if (text[index] == '-' && index + 1 < text.size() && text[index + 1] == '-') {
      out += "- -";
      index += 2;
      continue;
    }

    out += text[index];
    ++index;
  }

  return out;
}

std::string meta_tag(std::string_view attribute, std::string_view name, std::string_view content) {
  if (content.empty()) {
    return {};
  }

  std::string out = "<meta ";
  out += attribute;
  out += "=\"";
  escape_attribute(out, name);
  out += "\" content=\"";
  escape_attribute(out, content);
  out += "\"/>\n";

  return out;
}

void set_if_present(Value& object, std::string_view key, std::string_view text) {
  if (!text.empty()) {
    object.set(std::string(key), Value(std::string(text)));
  }
}

// A `<` only ever appears inside a JSON string, where \u003c means the same
// thing, so no title can close the script element early.
std::string structured_data(const Value& document) {
  std::string out = R"(<script type="application/ld+json">)";

  for (const char character : to_json(document)) {
    if (character == '<') {
      out += "\\u003c";
    } else {
      out += character;
    }
  }

  out += "</script>\n";

  return out;
}

Value nav_to_value(const std::vector<NavNode>& nodes, std::string_view section) {
  Value list = Value::array();

  for (const NavNode& node : nodes) {
    Value entry = Value::object();
    entry.set("name", Value(node.name));
    entry.set("label", Value(node.label));
    entry.set("path", Value(node.path));
    entry.set("url", Value(node.url));
    entry.set("current", Value(nav::is_current(node, section)));
    entry.set("children", nav_to_value(node.children, section));

    list.push(std::move(entry));
  }

  return list;
}

std::string site_base(const Chrome& chrome) {
  std::string base(chrome.site["base-url"].as_string());

  while (base.ends_with('/')) {
    base.pop_back();
  }

  return base;
}

std::string canonical_url(const Chrome& chrome) {
  return site_base(chrome) + chrome.url;
}

std::string share_image_url(const Chrome& chrome) {
  const std::string& url = chrome.share_image.url;

  return url.starts_with('/') && !url.starts_with("//") ? site_base(chrome) + url : url;
}

// The helpers a layout calls, as against the values it reads.
void define_chrome_functions(ViewContext& context, const Chrome& chrome) {
  context.define("framework-class", [framework = chrome.framework](const std::vector<ViewContext::Argument>& arguments) {
    if (arguments.empty()) {
      return Value(std::string{});
    }

    return Value(std::string(framework.class_for(arguments[0].value.as_string())));
  });

  // A nav item asks whether it is the one being looked at.
  context.define("nav-current", [](const std::vector<ViewContext::Argument>& arguments) {
    return arguments.empty() ? Value(false) : Value(arguments[0].value["current"].truthy());
  });

  const bool debug = chrome.debug;

  context.define("debug-open", [debug](const std::vector<ViewContext::Argument>& arguments) {
    if (!debug || arguments.empty()) {
      return Value(std::string{});
    }

    return Value("<!-- begin " + safe_comment(arguments[0].value.as_string()) + " -->\n");
  });

  context.define("debug-close", [debug](const std::vector<ViewContext::Argument>& arguments) {
    if (!debug || arguments.empty()) {
      return Value(std::string{});
    }

    return Value("<!-- end " + safe_comment(arguments[0].value.as_string()) + " -->\n");
  });

  context.define("format-date", [](const std::vector<ViewContext::Argument>& arguments) {
    if (arguments.empty()) {
      return Value(std::string{});
    }

    const std::string_view pattern = arguments.size() > 1 ? arguments[1].value.as_string() : "%Y-%m-%d";

    return Value(filters::format_date(arguments[0].value.as_string(), pattern));
  });

  context.define("truncate", [](const std::vector<ViewContext::Argument>& arguments) {
    if (arguments.empty()) {
      return Value(std::string{});
    }

    const auto length = arguments.size() > 1 ? static_cast<std::size_t>(arguments[1].value.as_integer(200))
                                             : std::size_t{200};

    return Value(filters::truncate(arguments[0].value.as_string(), length));
  });

  context.define("group-by", [](const std::vector<ViewContext::Argument>& arguments) {
    if (arguments.size() < 2) {
      return Value::array();
    }

    Value out = Value::array();

    for (const filters::Group& group : filters::group_by(arguments[0].value, arguments[1].value.as_string())) {
      Value items = Value::array();

      for (const Value& item : group.items) {
        items.push(item);
      }

      Value entry = Value::object();
      entry.set("key", Value(group.key));
      entry.set("items", std::move(items));

      out.push(std::move(entry));
    }

    return out;
  });
}

// Everything a layout can ask for that does not depend on which kind of page it
// is.
void add_chrome(ViewContext& context, const Chrome& chrome, const HeadMeta& page,
                std::string_view template_label) {
  context.set("site", chrome.site);
  context.set("site-title", chrome.site["title"]);
  context.set("data", chrome.data);
  context.set("languages", chrome.languages);
  context.set("has-languages", Value(!chrome.languages.empty()));
  context.set("section", Value(chrome.section));
  context.set("url", Value(chrome.url));
  context.set("nav-nodes", nav_to_value(chrome.nav, chrome.section));
  context.set("has-header", Value(chrome.has_header));
  context.set("has-sidebar", Value(chrome.has_sidebar));
  context.set("has-footer", Value(chrome.has_footer));
  context.set("debug", Value(chrome.debug));
  context.set("template-label", Value(std::string(template_label)));

  context.set("page-title", Value(page.title));
  context.set("meta-title", Value(compose_title(chrome.site["title"].as_string(), page.title)));
  context.set("meta-description", Value(page.description));
  context.set("meta-type", Value(page.type));
  context.set("canonical-url", Value(canonical_url(chrome)));
  context.set("section-label", Value(section_label(chrome.nav, chrome.section)));

  context.set("head-meta", Value(head_meta(chrome, page)));
  context.set("theme-script", Value(std::string(theme_script())));
  context.set("theme-toggle", Value(std::string(theme_toggle())));

  const std::string_view stylesheet = chrome.framework.stylesheet();
  const std::string_view script = chrome.framework.script();

  context.set("framework-stylesheet-tag",
              Value(stylesheet.empty() ? std::string{}
                                       : R"(<link rel="stylesheet" href=")" + std::string(stylesheet) + "\">"));
  context.set("framework-script-tag",
              Value(script.empty() ? std::string{} : "<script src=\"" + std::string(script) + "\"></script>"));

  define_chrome_functions(context, chrome);
}

}  // namespace

std::vector<int> pagination_window(int current, int total) {
  std::vector<int> pages;

  for (int page = std::max(1, current - 3); page <= std::min(total, current + 3); ++page) {
    pages.push_back(page);
  }

  return pages;
}

std::string compose_title(std::string_view site_title, std::string_view page_title) {
  if (page_title.empty() || page_title == site_title) {
    return std::string(site_title);
  }

  return std::string(site_title) + " :: " + std::string(page_title);
}

std::string section_label(const std::vector<NavNode>& nav, std::string_view section) {
  if (const NavNode* node = nav::find(nav, section)) {
    return node->label;
  }

  const auto slash = section.find_last_of('/');

  return slug::humanize(slash == std::string_view::npos ? section : section.substr(slash + 1));
}

std::string head_meta(const Chrome& chrome, const HeadMeta& page) {
  const std::string site_title(chrome.site["title"].as_string());
  const std::string title = page.title.empty() ? site_title : page.title;
  const std::string& description = page.description;
  const std::string& type = page.type;
  const std::string url = canonical_url(chrome);

  std::string out;

  if (!url.empty()) {
    out += R"(<link rel="canonical" href=")";
    escape_attribute(out, url);
    out += "\"/>\n";
  }

  if (chrome.noindex) {
    out += meta_tag("name", "robots", "noindex");
  }

  out += meta_tag("name", "description", description);
  out += meta_tag("property", "og:type", type);
  out += meta_tag("property", "og:title", title);
  out += meta_tag("property", "og:description", description);
  out += meta_tag("property", "og:url", url);
  out += meta_tag("property", "og:site_name", site_title);

  const std::string image = share_image_url(chrome);

  out += meta_tag("property", "og:image", image);

  if (!image.empty() && chrome.share_image.width > 0 && chrome.share_image.height > 0) {
    out += meta_tag("property", "og:image:width", std::format("{}", chrome.share_image.width));
    out += meta_tag("property", "og:image:height", std::format("{}", chrome.share_image.height));
  }

  if (type == "article") {
    out += meta_tag("property", "article:published_time", page.published);
    out += meta_tag("property", "article:modified_time", page.modified);

    for (const std::string& tag : page.tags) {
      out += meta_tag("property", "article:tag", tag);
    }
  }

  const std::string_view twitter = chrome.site["twitter"].as_string();

  out += meta_tag("name", "twitter:card", image.empty() ? "summary" : "summary_large_image");
  out += meta_tag("name", "twitter:site", twitter);
  out += meta_tag("name", "twitter:creator", twitter);
  out += meta_tag("name", "twitter:title", title);
  out += meta_tag("name", "twitter:description", description);

  for (const FeedLink& feed : chrome.feeds) {
    out += R"(<link rel="alternate" type=")";
    escape_attribute(out, feed.type);
    out += R"(" title=")";
    escape_attribute(out, site_title);
    out += R"(" href=")";
    escape_attribute(out, site_base(chrome) + feed.url);
    out += "\"/>\n";
  }

  if (type == "article") {
    Value posting = Value::object();
    posting.set("@context", Value("https://schema.org"));
    posting.set("@type", Value("BlogPosting"));
    posting.set("headline", Value(title));
    posting.set("url", Value(url));
    set_if_present(posting, "description", description);
    set_if_present(posting, "datePublished", page.published);
    set_if_present(posting, "dateModified", page.modified);
    set_if_present(posting, "image", image);

    if (const std::string_view author = chrome.site["author"].as_string(); !author.empty()) {
      Value person = Value::object();
      person.set("@type", Value("Person"));
      person.set("name", Value(std::string(author)));
      posting.set("author", std::move(person));
    }

    out += structured_data(posting);
  }

  if (chrome.home) {
    Value website = Value::object();
    website.set("@context", Value("https://schema.org"));
    website.set("@type", Value("WebSite"));
    website.set("name", Value(site_title));
    website.set("url", Value(url));

    out += structured_data(website);
  }

  return out;
}

std::string pagination_html(const ListingView& listing) {
  const auto total = static_cast<int>(listing.page_urls.size());

  if (total <= 1) {
    return {};
  }

  const Framework& framework = listing.chrome.framework;

  const auto classes = [](std::initializer_list<std::string_view> names) {
    std::string joined;

    for (const std::string_view name : names) {
      if (!name.empty()) {
        joined += joined.empty() ? std::string(name) : " " + std::string(name);
      }
    }

    return joined.empty() ? std::string{} : " class=\"" + joined + "\"";
  };

  const std::string item_class = classes({framework.class_for("pagination-item")});
  const std::string link_class = classes({framework.class_for("pagination-link")});

  std::string out = "<nav" + classes({"blogin-pagination", framework.class_for("pagination-nav")}) +
                    " aria-label=\"Pagination\"><ul" + classes({framework.class_for("pagination-list")}) + ">";

  const auto anchor = [&](std::string_view url, std::string_view label, std::string_view rel,
                          std::string_view aria) {
    std::string link = "<a" + link_class;

    if (!rel.empty()) {
      link += " rel=\"" + std::string(rel) + "\"";
    }

    if (!aria.empty()) {
      link += " aria-label=\"" + std::string(aria) + "\"";
    }

    link += " href=\"";
    escape_attribute(link, url);
    link += "\">" + std::string(label) + "</a>";

    return "<li" + item_class + ">" + link + "</li>";
  };

  if (!listing.previous_url.empty()) {
    out += anchor(listing.page_urls.front(), "&laquo;", "", "First");
    out += anchor(listing.previous_url, "&lsaquo;", "prev", "Previous");
  }

  for (const int page : pagination_window(listing.page_number, total)) {
    if (page == listing.page_number) {
      out += "<li" + classes({framework.class_for("pagination-item"),
                              framework.class_for("pagination-active-item")}) +
             "><span" + classes({framework.class_for("pagination-link"),
                                 framework.class_for("pagination-active-link")}) +
             " aria-current=\"page\">" + std::format("{}", page) + "</span></li>";
      continue;
    }

    out += anchor(listing.page_urls[static_cast<std::size_t>(page - 1)], std::format("{}", page), "", "");
  }

  if (!listing.next_url.empty()) {
    out += anchor(listing.next_url, "&rsaquo;", "next", "Next");
    out += anchor(listing.page_urls.back(), "&raquo;", "", "Last");
  }

  return out + "</ul></nav>";
}

std::string post_navigation_html(const PostView& page, const Framework& framework) {
  if (page.previous_url.empty() && page.next_url.empty()) {
    return {};
  }

  std::string button(framework.class_for("post-nav-button"));

  if (button.empty()) {
    button = "blogin-btn";
  }

  std::string out = "<nav class=\"post-nav\">";

  if (!page.previous_url.empty()) {
    out += "<a class=\"prev " + button + "\" href=\"";
    escape_attribute(out, page.previous_url);
    out += R"("><span aria-hidden="true">&larr;</span> <span class="post-nav-label">)";
    escape_attribute(out, page.previous_title);
    out += "</span></a>";
  }

  if (!page.next_url.empty()) {
    out += "<a class=\"next " + button + "\" href=\"";
    escape_attribute(out, page.next_url);
    out += R"("><span class="post-nav-label">)";
    escape_attribute(out, page.next_title);
    out += "</span> <span aria-hidden=\"true\">&rarr;</span></a>";
  }

  return out + "</nav>";
}

ViewContext build(const PostView& page) {
  ViewContext context;

  const std::string description =
    page.post != nullptr && !page.post->description.empty() ? page.post->description : page.summary;

  HeadMeta meta;
  meta.description = description;
  meta.type = "article";

  if (page.post != nullptr) {
    meta.title = page.post->title;
    meta.published = page.post->date_string();
    meta.modified = page.post->updated_string();
    meta.tags = page.post->tags;
  }

  add_chrome(context, page.chrome, meta, "template: show");

  if (page.post != nullptr) {
    context.set("title", Value(page.post->title));
    context.set("date", Value(page.post->date_string()));
    context.set("description", Value(page.post->description));
    context.set("slug", Value(page.post->slug));
    context.set("has-toc", Value(page.post->toc));
  }

  context.set("body", Value(page.body_html));
  context.set("summary", Value(page.summary));
  context.set("show-dates", Value(page.show_dates));
  context.set("index-dates", Value(page.index_dates));
  context.set("word-count", Value(static_cast<std::int64_t>(page.word_count)));
  context.set("reading-time", Value(static_cast<std::int64_t>(page.reading_time)));
  context.set("related", page.related);
  context.set("has-related", Value(!page.related.empty()));
  context.set("tags", page.tags);
  context.set("has-tags", Value(!page.tags.empty()));
  context.set("toc-html", Value(toc::render(toc::build(page.headings))));
  context.set("post-nav-html", Value(post_navigation_html(page, page.chrome.framework)));

  return context;
}

ViewContext build(const ListingView& listing) {
  ViewContext context;

  const std::string heading =
    listing.at_root ? std::string{}
                    : (listing.heading.empty() ? section_label(listing.chrome.nav, listing.chrome.section)
                                               : listing.heading);

  HeadMeta meta;
  meta.title = heading;
  meta.type = "website";

  add_chrome(context, listing.chrome, meta, "template: index");

  context.set("posts", listing.entries);
  context.set("entries", listing.entries);
  context.set("heading", Value(heading.empty() ? section_label(listing.chrome.nav, listing.chrome.section)
                                               : heading));
  context.set("page-number", Value(static_cast<std::int64_t>(listing.page_number)));
  context.set("total-pages", Value(static_cast<std::int64_t>(listing.total_pages)));
  context.set("index-dates", Value(listing.index_dates));
  context.set("at-root", Value(listing.at_root));
  context.set("pagination-html", Value(pagination_html(listing)));

  Value links = Value::array();

  if (listing.page_urls.size() > 1) {
    for (std::size_t index = 0; index < listing.page_urls.size(); ++index) {
      Value entry = Value::object();
      entry.set("number", Value(static_cast<std::int64_t>(index + 1)));
      entry.set("url", Value(listing.page_urls[index]));
      entry.set("current", Value(std::cmp_equal(index + 1, listing.page_number)));

      links.push(std::move(entry));
    }
  }

  context.set("pagination-links", std::move(links));

  return context;
}

}  // namespace blogin::view
