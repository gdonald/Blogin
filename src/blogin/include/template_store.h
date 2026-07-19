#pragma once

#include <atomic>
#include <expected>
#include <shared_mutex>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haml.h"

namespace blogin {

// Every template a site can reach, compiled once.
//
// Compilation happens up front rather than on first use, so rendering is a pure
// read and any number of threads can render at once without a lock. A build
// compiles a few dozen templates and renders thousands of pages from them.
class TemplateStore {
public:
  // Earlier paths win, which is how a site's own layout overrides a theme's.
  static std::expected<TemplateStore, ParseError> load(const std::vector<std::filesystem::path>& search_paths);

  // A section's own directory answers first, then each directory above it, then
  // the layout root. `docs/ORM-ActiveRecord/_docnav.haml` is what a post in
  // that section renders, and `docs/MVC-Keayl/_docnav.haml` is what a post in
  // that one renders, though both are named `docnav`.
  const haml::Template* find(std::string_view name, std::string_view section = {}) const;

  const haml::Template* find_partial(std::string_view name, std::string_view section = {}) const;

  bool has(std::string_view name, std::string_view section = {}) const {
    return find(name, section) != nullptr;
  }

  bool has_partial(std::string_view name, std::string_view section = {}) const {
    return find_partial(name, section) != nullptr;
  }

  std::size_t size() const { return templates_.size(); }

  // The names, sorted, for reporting what a site carries.
  std::vector<std::string> names() const;

  // Whether any template's source contains this text. It answers whether a
  // value the build would have to work out is asked for by anything, so a site
  // that never mentions related posts does not pay for them.
  //
  // A word in a comment or in prose counts, which is the safe direction: the
  // value is worked out when it might be read, never skipped when it is.
  bool mentions(std::string_view word) const;

private:
  std::unordered_map<std::string, haml::Template> templates_;

  // Every template's source, joined, for `mentions`.
  std::string sources_;
};

// A fragment rendered once and reused wherever it would come out the same.
//
// The key is derived from what the fragment read, not from the name the author
// gave it, so a fragment that turns out to depend on the page is never reused
// across pages even if somebody asked for it to be.
//
// Shared by every rendering thread, so it locks. A page looks a fragment up a
// handful of times rather than once per node, and after the first few pages
// almost every lookup is a read, so a shared lock costs close to nothing. The
// alternative, one cache per thread, would render the site's chrome once per
// thread instead of once.
class FragmentCache : public haml::FragmentStore {
public:
  const std::string* find(std::string_view key) const override;

  void store(std::string key, std::string html) override;

  const std::vector<std::string>* reads(const void* site) const override;

  void remember_reads(const void* site, std::vector<std::string> names) override;

  void note_render() override;

  void clear();

  std::size_t size() const { return entries_.size(); }

  std::size_t hits() const { return hits_.load(std::memory_order_relaxed); }

  std::size_t misses() const { return misses_.load(std::memory_order_relaxed); }

  // Fragment bodies rendered. A reused fragment costs a lookup, so this is the
  // count that says the site's chrome was built once rather than once per page.
  std::size_t renders() const { return renders_.load(std::memory_order_relaxed); }

private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::string> entries_;

  mutable std::shared_mutex reads_mutex_;
  std::unordered_map<const void*, std::vector<std::string>> reads_;

  mutable std::atomic<std::size_t> hits_{0};
  mutable std::atomic<std::size_t> misses_{0};
  std::atomic<std::size_t> renders_{0};
};

}  // namespace blogin
