#include "template_store.h"
#include "files.h"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>

#include "counters.h"
#include "text.h"

namespace blogin {
namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  if (!input) {
    return {};
  }

  count(Counter::files_read);

  return files::read_file(path);
}

// "show.haml" and "show.html.haml" both name the template "show"; "_entry.haml"
// names the partial "_entry". A file in a subdirectory keeps that directory in
// its name, so "docs/ORM-ActiveRecord/_docnav.haml" is a different template
// from "docs/MVC-Keayl/_docnav.haml" rather than the same one twice.
std::string template_name(const std::filesystem::path& path, const std::filesystem::path& root) {
  std::string relative = std::filesystem::relative(path, root).generic_string();

  for (const std::string_view suffix : {".html.haml", ".haml"}) {
    if (relative.ends_with(suffix)) {
      relative = relative.substr(0, relative.size() - suffix.size());
      break;
    }
  }

  return relative;
}

}  // namespace

std::expected<TemplateStore, ParseError> TemplateStore::load(const std::vector<std::filesystem::path>& search_paths) {
  TemplateStore store;

  for (const std::filesystem::path& directory : search_paths) {
    std::error_code error;

    if (!std::filesystem::is_directory(directory, error)) {
      continue;
    }

    std::vector<std::filesystem::path> files;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, error)) {
      if (entry.is_regular_file() && entry.path().extension() == ".haml") {
        files.push_back(entry.path());
      }
    }

    std::sort(files.begin(), files.end());

    for (const std::filesystem::path& file : files) {
      const std::string name = template_name(file, directory);

      // Earlier search paths win, so a site's own layout overrides a theme's.
      if (store.templates_.contains(name)) {
        continue;
      }

      std::string source = read_file(file);

      store.sources_ += source;
      store.sources_ += '\n';

      auto compiled = haml::Template::compile(std::move(source), file.string());

      if (!compiled) {
        ParseError failure = compiled.error();
        failure.message = file.string() + ": " + failure.message;

        return std::unexpected(std::move(failure));
      }

      count(Counter::templates_compiled);
      store.templates_.emplace(name, std::move(*compiled));
    }
  }

  return store;
}

const haml::Template* TemplateStore::find(std::string_view name, std::string_view section) const {
  // The section's own directory, then each one above it, then the root.
  std::string directory(section);

  for (;;) {
    const auto found = templates_.find(directory.empty() ? std::string(name)
                                                         : directory + "/" + std::string(name));

    if (found != templates_.end()) {
      return &found->second;
    }

    if (directory.empty()) {
      return nullptr;
    }

    const auto separator = directory.rfind('/');

    directory = separator == std::string::npos ? std::string{} : directory.substr(0, separator);
  }
}

const haml::Template* TemplateStore::find_partial(std::string_view name,
                                                  std::string_view section) const {
  return find("_" + std::string(name), section);
}

bool TemplateStore::mentions(std::string_view word) const {
  return sources_.contains(word);
}

std::vector<std::string> TemplateStore::names() const {
  std::vector<std::string> out;

  out.reserve(templates_.size());
  for (const auto& entry : templates_) {
    out.push_back(entry.first);
  }

  std::sort(out.begin(), out.end());

  return out;
}

const std::string* FragmentCache::find(std::string_view key) const {
  const std::shared_lock guard(mutex_);

  const auto found = entries_.find(std::string(key));

  if (found == entries_.end()) {
    misses_.fetch_add(1, std::memory_order_relaxed);

    return nullptr;
  }

  hits_.fetch_add(1, std::memory_order_relaxed);

  // Entries are never removed or replaced while the build runs, so the pointer
  // stays good after the lock is released.
  return &found->second;
}

void FragmentCache::store(std::string key, std::string html) {
  const std::unique_lock guard(mutex_);

  entries_.emplace(std::move(key), std::move(html));
}

const std::vector<std::string>* FragmentCache::reads(const void* site) const {
  const std::shared_lock guard(reads_mutex_);

  const auto found = reads_.find(site);

  // Recorded once and never replaced, so the pointer stays good after the lock
  // is released and a rehash does not move what it points at.
  return found == reads_.end() ? nullptr : &found->second;
}

void FragmentCache::remember_reads(const void* site, std::vector<std::string> names) {
  const std::unique_lock guard(reads_mutex_);

  reads_.emplace(site, std::move(names));
}

void FragmentCache::note_render() {
  renders_.fetch_add(1, std::memory_order_relaxed);
}

void FragmentCache::clear() {
  const std::unique_lock guard(mutex_);
  const std::unique_lock reads_guard(reads_mutex_);

  entries_.clear();
  reads_.clear();
  hits_.store(0, std::memory_order_relaxed);
  misses_.store(0, std::memory_order_relaxed);
  renders_.store(0, std::memory_order_relaxed);
}

}  // namespace blogin
