#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "framework.h"
#include "nav.h"
#include "post.h"
#include "toc.h"
#include "value.h"
#include "view_context.h"

namespace blogin {

// What every page has, whatever kind of page it is.
struct Chrome {
  Value site = Value::object();
  Value data = Value::object();
  Value languages = Value::array();

  std::string section;
  std::string url;

  std::vector<NavNode> nav;

  bool has_header = false;
  bool has_sidebar = false;
  bool has_footer = false;

  // Wraps each rendered region in a comment naming what produced it, so a page
  // that came out wrong says which template to look at.
  bool debug = false;

  Framework framework = Framework::profile("none");
};

struct PostView {
  Chrome chrome;

  const Post* post = nullptr;
  std::string body_html;
  std::string summary;
  std::vector<Heading> headings;

  std::size_t word_count = 0;
  int reading_time = 0;

  Value related = Value::array();
  Value tags = Value::array();

  bool show_dates = true;

  // What a listing in this section would do with dates. A post page renders
  // `related` through the same entry partial a listing uses, and that partial
  // reads `index-dates`, so the name has to resolve on both contexts.
  bool index_dates = true;

  std::string previous_url;
  std::string previous_title;
  std::string next_url;
  std::string next_title;
};

struct ListingView {
  Chrome chrome;

  Value entries = Value::array();

  int page_number = 1;
  int total_pages = 1;

  std::string previous_url;
  std::string next_url;
  std::vector<std::string> page_urls;

  bool index_dates = true;
  std::string heading;

  // The home listing has no heading of its own, since the site title is already
  // above it.
  bool at_root = false;
};

namespace view {

// The numbered range a pagination bar shows: the current page and up to three
// either side, clamped. First, last, previous, and next are separate controls,
// so the numbered part never grows past seven.
std::vector<int> pagination_window(int current, int total);

std::string compose_title(std::string_view site_title, std::string_view page_title);

// The label a section shows in a menu, from its nav entry or from its name.
std::string section_label(const std::vector<NavNode>& nav, std::string_view section);

std::string head_meta(const Chrome& chrome, std::string_view page_title, std::string_view description,
                      std::string_view type);

std::string pagination_html(const ListingView& listing);

std::string post_navigation_html(const PostView& page, const Framework& framework);

std::string_view theme_script();

std::string_view theme_toggle();

ViewContext build(const PostView& page);

ViewContext build(const ListingView& listing);

}  // namespace view
}  // namespace blogin
