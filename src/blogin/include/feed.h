#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace blogin {

// One entry as a feed sees it. Feeds do not know about posts, only about what
// they publish.
struct FeedEntry {
  std::string title;
  std::string url;
  std::string date;
  std::string summary;
};

struct FeedInfo {
  std::string title;
  std::string site_url;
  std::string feed_url;
  std::string updated;
  std::vector<FeedEntry> entries;
};

namespace feed {

std::string atom(const FeedInfo& info);

std::string rss(const FeedInfo& info);

std::string json_feed(const FeedInfo& info);

std::string sitemap(const std::vector<std::string>& locations);

std::string robots_txt(std::string_view base_url);

// "feed.xml", "rss.xml", or "feed.json".
std::string_view filename_for(std::string_view format);

}  // namespace feed
}  // namespace blogin
