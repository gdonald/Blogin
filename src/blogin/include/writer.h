#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blogin {

// Writes the output tree, skipping what has not changed.
//
// Change is decided by a hash carried between builds, not by reading the
// previous file back off disk, so a build that rewrites nothing does no reading
// either. Every write goes to a temporary file and is renamed into place, so a
// build killed partway leaves either the old page or the new one.
class Writer {
public:
  explicit Writer(std::filesystem::path root, bool force = false);

  // Loads the manifest the last build left behind. A missing or unreadable one
  // is not an error, and means everything is written this time.
  void load_manifest();

  void save_manifest() const;

  void write(const std::filesystem::path& path, std::string_view content);

  // Applied to every page before it is hashed and written. Fingerprinted asset
  // urls are substituted here, not at each of the dozen places a page is
  // produced, so a new kind of page cannot forget to do it.
  //
  // Set once, before any concurrent write, and pure: it is called from every
  // render thread.
  void set_page_filter(std::function<std::string(std::string_view)> filter);

  void copy(const std::filesystem::path& from, const std::filesystem::path& to);

  // Records a file changed outside write and copy, so pruning still knows about
  // it.
  void record(const std::filesystem::path& path);

  // Keeps a file the build did not regenerate: it is still expected output, and
  // its hash carries forward, but nothing is written or read, so an incremental
  // build can leave a page alone without re-rendering it to learn it is the same.
  bool keep(const std::filesystem::path& path);

  // Whether keeping that file would succeed, asked without committing to it.
  // A post's page can only be left alone once its neighbours are known, which
  // is after the point where the rest of the decision is made.
  bool reusable(const std::filesystem::path& path);

  // Removes anything under the root that this build did not account for, and
  // then any directory left empty. A .keep file is the site owner's, not build
  // output, so it survives.
  void prune();

  std::size_t written() const { return written_; }

  std::size_t skipped() const { return skipped_; }

  const std::vector<std::filesystem::path>& changed() const { return changed_; }

  // Paths produced twice in one build with different content. One of the two
  // results reaches disk and which one is an accident of ordering, so this is
  // reported, not resolved. Writing the same bytes twice is harmless and
  // not counted.
  const std::vector<std::string>& collisions() const { return collisions_; }

  // Where the sequence of produced paths currently stands, and everything
  // produced since a given point. A build uses this to remember which outputs
  // came from a section of work, so a later build can carry them forward
  // without repeating it.
  std::size_t mark() const;

  std::vector<std::string> since(std::size_t start) const;

private:
  std::string relative(const std::filesystem::path& path) const;

  std::filesystem::path root_;
  bool force_ = false;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::string> previous_;
  std::unordered_map<std::string, std::string> current_;
  std::unordered_set<std::string> expected_;
  std::vector<std::filesystem::path> changed_;
  std::vector<std::string> collisions_;
  std::vector<std::string> produced_;
  std::function<std::string(std::string_view)> page_filter_;

  std::size_t written_ = 0;
  std::size_t skipped_ = 0;
};

// A stable hash of some bytes, used for change detection and for asset
// fingerprints.
std::string content_hash(std::string_view content);

}  // namespace blogin
