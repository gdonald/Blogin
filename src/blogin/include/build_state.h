#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "value.h"

namespace blogin {

// What a build remembers about one source file, so the next build can decide
// whether anything about it changed without reading it.
//
// The metadata is here because listings, feeds, and the search index all read
// it. Without it an unchanged post would still have to be parsed to learn
// its title, and a rebuild would be incremental in name only.
struct SourceState {
  std::uintmax_t size = 0;
  std::int64_t modified = 0;
  std::string hash;

  std::string output;
  Value metadata = Value::object();
};

// Everything carried from one build to the next.
class BuildState {
public:
  static BuildState load(const std::filesystem::path& path);

  void save(const std::filesystem::path& path) const;

  // A change to configuration, a layout, a data file, or a shortcode can affect
  // any page, so it is one value and any difference rebuilds everything. That is
  // fast in the common case and correct in every case.
  std::string fingerprint;

  std::unordered_map<std::string, SourceState> sources;

  // Output that is a function of the posts collectively, not of any one:
  // listings, taxonomies, feeds, the sitemap, and the search index. When no post
  // was added, removed, or reparsed, these are what the last build produced, so
  // they are carried forward without being rendered again.
  std::vector<std::string> derived;

  // How many listings produced them, so a carried-forward build still reports
  // what the site contains.
  std::int64_t listings = 0;

  // Files copied through, not rendered from, keyed by where they land.
  std::unordered_map<std::string, SourceState> copies;

  // When the build that wrote this state began.
  //
  // A file modified during a build, after that build stamped it, has the stamp
  // of a file that was already accounted for. Size and time would say it is
  // unchanged forever after, and the edit would never appear. So a source whose
  // recorded time is not safely older than this is not trusted on its stamp,
  // and its bytes are compared instead. Human editing is far too slow to land
  // in that window. A script writing a file the moment a build reads it is not.
  std::int64_t started = 0;

  // True when this source's stamp can be believed without reading it.
  bool settled(const SourceState& source) const { return source.modified < started; }
};

// The hash a source is remembered by, over its bytes, not its stamp.
std::string source_hash(const std::filesystem::path& path);

// Now, on the same clock the file stamps use.
std::int64_t stamp_now();

// Size and modification time of a file, cheap enough to ask about every source
// on every build.
std::pair<std::uintmax_t, std::int64_t> file_stamp(const std::filesystem::path& path);

}  // namespace blogin
