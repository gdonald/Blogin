#include "scaffold.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <ios>

#include "files.h"
#include "slug.h"

namespace blogin::scaffold {
namespace {

constexpr std::string_view base_haml = R"HAML(!!! 5
%html{lang: 'en'}
  %head
    %meta{charset: 'utf-8'}
    %meta{name: 'viewport', content: 'width=device-width, initial-scale=1'}
    %title= site-title
    != head-meta
    != framework-stylesheet-tag
    %link{rel: 'stylesheet', href: '/assets/css/blogin.css'}
    %link{rel: 'stylesheet', href: '/assets/css/style.css'}
    != theme-script
  %body
    - if has-header
      != debug-open('partial: header')
      != render(:partial<header>, :locals({brand: site-title}))
      != debug-close('partial: header')
    %main{class: framework-class('container')}
      != debug-open(template-label)
      = yield
      != debug-close(template-label)
    - if has-footer
      != debug-open('partial: footer')
      != render(:partial<footer>)
      != debug-close('partial: footer')
    != framework-script-tag
)HAML";

constexpr std::string_view show_haml = R"HAML(%article
  %h1= title
  - if show-dates
    %p.meta= "#{date} · #{reading-time} min read"
  - if has-toc
    %nav.toc
      != toc-html
  != body
  - if has-tags
    %nav.tags
      %ul
        - for tags -> $tag
          %li
            %a{href: "#{$tag<url>}"}= $tag<name>
!= post-nav-html
- if has-related
  %nav.related
    %h2 Related posts
    %ul
      != render(:partial<entry>, :collection(related), :as<entry>)
)HAML";

constexpr std::string_view not_found_haml = R"HAML(%section.not-found
  %h1 404
  %p The page you are looking for was not found.
  %p
    %a{href: '/'} Home
)HAML";

constexpr std::string_view index_haml = R"HAML(%section.listing
  %h1= heading
  %ul
    != render(:partial<entry>, :collection(posts), :as<entry>)
  != pagination-html
)HAML";

constexpr std::string_view entry_haml = R"HAML(%li
  %a{href: "#{$entry<url>}"}= $entry<title>
  - if index-dates
    %span.date= $entry<date>
)HAML";

constexpr std::string_view header_haml = R"HAML(%header
  %a.brand{href: '/'}= $brand
  != render(:partial<nav>)
  .navbar-tools
    .navbar-search
      != render(:partial<search>)
    .navbar-toggle-slot
      != theme-toggle
)HAML";

constexpr std::string_view nav_haml = R"HAML(%nav
  %ul
    != render(:partial<nav-item>, :collection(nav-nodes), :as<node>)
)HAML";

constexpr std::string_view nav_item_haml = R"HAML(%li
  - if nav-current($node)
    %a.current{href: "#{$node.url}"}= $node.label
  - else
    %a{href: "#{$node.url}"}= $node.label
  - if $node.children.elems
    %ul
      != render(:partial<nav-item>, :collection($node.children), :as<node>)
)HAML";

constexpr std::string_view footer_haml = R"HAML(%footer
  %p
    Built with
    %a{href: 'https://blogin.dev'} Blogin
  %nav.feeds
    %a{href: '/feed.xml'} Atom
    %a{href: '/rss.xml'} RSS
)HAML";

constexpr std::string_view search_haml = R"HAML(.blogin-search
  %form{'data-blogin-search' => 'true'}
    %input{type: 'search', name: 'q', placeholder: 'Search'}
  %ul{'data-blogin-results' => 'true'}
%link{rel: 'stylesheet', href: '/assets/css/search.css'}
%script{src: '/assets/js/search.js'}
)HAML";

constexpr std::string_view bootstrap_base_haml = R"HAML(!!! 5
%html{lang: 'en'}
  %head
    %meta{charset: 'utf-8'}
    %meta{name: 'viewport', content: 'width=device-width, initial-scale=1'}
    %title= site-title
    != head-meta
    != framework-stylesheet-tag
    %link{rel: 'stylesheet', href: '/assets/css/blogin.css'}
    %link{rel: 'stylesheet', href: '/assets/css/style.css'}
    != theme-script
  %body.d-flex.flex-column.min-vh-100
    - if has-header
      != debug-open('partial: header')
      != render(:partial<header>, :locals({brand: site-title}))
      != debug-close('partial: header')
    %main.container.my-4.flex-grow-1
      != debug-open(template-label)
      = yield
      != debug-close(template-label)
    - if has-footer
      != debug-open('partial: footer')
      != render(:partial<footer>)
      != debug-close('partial: footer')
    != framework-script-tag
)HAML";

constexpr std::string_view bootstrap_header_haml = R"HAML(%nav.navbar.navbar-expand-lg.bg-body-tertiary.border-bottom
  .container
    %a.navbar-brand{href: '/'}= $brand
    %button.navbar-toggler{type: 'button', 'data-bs-toggle' => 'collapse', 'data-bs-target' => '#topnav', 'aria-controls' => 'topnav', 'aria-expanded' => 'false', 'aria-label' => 'Toggle navigation'}
      %span.navbar-toggler-icon
    #topnav.collapse.navbar-collapse
      != render(:partial<nav>)
      .navbar-tools
        .navbar-search
          != render(:partial<search>)
        .navbar-toggle-slot
          != theme-toggle
)HAML";

constexpr std::string_view bootstrap_nav_haml = R"HAML(%ul.navbar-nav.me-auto.mb-2.mb-lg-0
  != render(:partial<nav-item>, :collection(nav-nodes), :as<node>)
)HAML";

constexpr std::string_view bootstrap_nav_item_haml = R"HAML(%li.nav-item
  - if nav-current($node)
    %a.nav-link.active{href: "#{$node.url}"}= $node.label
  - else
    %a.nav-link{href: "#{$node.url}"}= $node.label
)HAML";

constexpr std::string_view bootstrap_footer_haml = R"HAML(%footer.border-top.py-3.mt-auto
  .container.d-flex.flex-wrap.justify-content-between.gap-2
    %p.text-body-secondary.mb-0
      Built with
      %a.link-secondary{href: 'https://blogin.dev'} Blogin
    %nav.d-flex.gap-3
      %a.link-secondary{href: '/feed.xml'} Atom
      %a.link-secondary{href: '/rss.xml'} RSS
)HAML";

constexpr std::string_view bootstrap_index_haml = R"HAML(%section
  %h1.mb-4= heading
  .list-group
    != render(:partial<entry>, :collection(posts), :as<entry>)
  != pagination-html
)HAML";

constexpr std::string_view bootstrap_entry_haml = R"HAML(%a.list-group-item.list-group-item-action{href: "#{$entry<url>}"}
  %span= $entry<title>
  - if index-dates
    %span.text-body-secondary.ms-2= $entry<date>
)HAML";

constexpr std::string_view plain_stylesheet = R"HAML(:root {
  --blogin-bg: #ffffff;
  --blogin-fg: #1a1d24;
  --blogin-link: #0d6efd;
  --blogin-border: #e5e7eb;
}

[data-theme="dark"] {
  --blogin-bg: #0d1117;
  --blogin-fg: #e6edf3;
  --blogin-link: #58a6ff;
  --blogin-border: #30363d;
}

body {
  margin: 0;
  background: var(--blogin-bg);
  color: var(--blogin-fg);
}

a { color: var(--blogin-link); }

header, footer { border-color: var(--blogin-border); }

header {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem 1rem;
}
)HAML";

std::string configuration(std::string_view framework) {
  // Deliberately without a "plugins" key: there is no plugin system here, and a
  // key a build would reject has no business in the file it writes for you.
  return std::format(R"JSON({{
  "title": "My Blogin Site",
  "base-url": "https://example.com",
  "author": "",
  "output-dir": "public",
  "home-section": "posts",
  "clean-urls": false,
  "css-framework": "{}",
  "page-size": 10,
  "highlight": false,
  "summary-length": 200,
  "reading-wpm": 200,
  "related-count": 5,
  "taxonomies": ["tags"],
  "feed-formats": ["atom", "rss"],
  "robots": true,
  "minify": false,
  "fingerprint": false,
  "image-widths": [],
  "search": true,
  "search-text-length": 2000,
  "search-cap": 10,
  "languages": [],
  "language-config": {{}},
  "theme": "",
  "debug": false,
  "sections": {{}}
}}
)JSON",
                     framework);
}

std::string starter_post(std::string_view date) {
  return std::format(R"POST(---
title: Hello World
date: {}
tags: [intro]
description: The first post on your Blogin site.
---
Welcome to your new **Blogin** site.

Edit the Markdown under `content/`, tweak the HAML in `layouts/`, then run
`blogin build` to regenerate `public/`.
)POST",
                     date);
}

std::string post_stub(std::string_view title, std::string_view date) {
  return std::format(R"POST(---
title: "{}"
date: {}
tags: []
description:
---
Write your post here.
)POST",
                     title, date);
}

void write_file(const std::filesystem::path& path, std::string_view content) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

}  // namespace

const std::vector<std::string>& known_frameworks() {
  // Every profile the renderer ships can be scaffolded. Bootstrap gets layouts
  // of its own because its navbar and grid need markup no other framework
  // wants. None, Pico, and Bulma share the plain layouts: Pico styles bare
  // semantic HTML, Bulma styles the rendered body through the wrapper the
  // renderer adds, and both take their page width from the container slot.
  static const std::vector<std::string> frameworks{"none", "bootstrap5", "pico", "bulma"};

  return frameworks;
}

std::string today() {
  const auto now = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());

  return std::format("{:%Y-%m-%d}", std::chrono::year_month_day{now});
}

std::map<std::string, std::string> files(std::string_view framework, std::string_view date) {
  std::map<std::string, std::string> written{
    {"blogin.json", configuration(framework)},
    {std::format("content/posts/{}-hello-world.md", date), starter_post(date)},
    {"layouts/base.haml", std::string(base_haml)},
    {"layouts/show.haml", std::string(show_haml)},
    {"layouts/index.haml", std::string(index_haml)},
    {"layouts/404.haml", std::string(not_found_haml)},
    {"layouts/_entry.haml", std::string(entry_haml)},
    {"layouts/_header.haml", std::string(header_haml)},
    {"layouts/_nav.haml", std::string(nav_haml)},
    {"layouts/_nav-item.haml", std::string(nav_item_haml)},
    {"layouts/_footer.haml", std::string(footer_haml)},
    {"layouts/_search.haml", std::string(search_haml)},
    {"assets/css/style.css", framework == "none" ? std::string(plain_stylesheet) : std::string{}},
  };

  if (framework == "bootstrap5") {
    written["layouts/base.haml"] = std::string(bootstrap_base_haml);
    written["layouts/index.haml"] = std::string(bootstrap_index_haml);
    written["layouts/_entry.haml"] = std::string(bootstrap_entry_haml);
    written["layouts/_header.haml"] = std::string(bootstrap_header_haml);
    written["layouts/_nav.haml"] = std::string(bootstrap_nav_haml);
    written["layouts/_nav-item.haml"] = std::string(bootstrap_nav_item_haml);
    written["layouts/_footer.haml"] = std::string(bootstrap_footer_haml);
  }

  return written;
}

std::expected<std::filesystem::path, ParseError> init(const std::filesystem::path& directory,
                                                      std::string_view framework, bool force,
                                                      std::string_view date) {
  const std::vector<std::string>& frameworks = known_frameworks();

  if (std::find(frameworks.begin(), frameworks.end(), framework) == frameworks.end()) {
    std::string known;

    for (const std::string& name : frameworks) {
      known += known.empty() ? name : ", " + name;
    }

    return std::unexpected(
      ParseError{std::format("unknown framework '{}' (known: {})", framework, known), 1, 1});
  }

  std::error_code error;

  // Whatever is already here belongs to someone else.
  if (std::filesystem::is_directory(directory, error) && !force) {
    std::vector<std::string> existing;

    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
      existing.push_back(entry.path().filename().string());
    }

    if (!existing.empty()) {
      std::sort(existing.begin(), existing.end());

      std::string names;

      for (const std::string& name : existing) {
        names += names.empty() ? name : ", " + name;
      }

      return std::unexpected(ParseError{
        std::format("target '{}' is not empty ({}). Pass --force to write into it anyway",
                    directory.string(), names),
        1, 1});
    }
  }

  for (const auto& entry : files(framework, date)) {
    write_file(directory / entry.first, entry.second);
  }

  // Empty on purpose, so a new site has somewhere obvious to put these.
  for (const std::string_view name : {"assets/js", "assets/img"}) {
    std::filesystem::create_directories(directory / name, error);
  }

  return directory;
}

std::expected<std::filesystem::path, ParseError> new_post(std::string_view title,
                                                          const std::filesystem::path& content,
                                                          std::string_view section, std::string_view date,
                                                          bool force) {
  const std::filesystem::path directory = section.empty() ? content : content / section;
  std::filesystem::path file =
    directory / std::format("{}-{}.md", date, slug::slugify(title));

  std::error_code error;

  if (std::filesystem::exists(file, error) && !force) {
    return std::unexpected(
      ParseError{std::format("post already exists: {} (pass --force to overwrite it)", file.string()), 1, 1});
  }

  write_file(file, post_stub(title, date));

  return file;
}

}  // namespace blogin::scaffold
