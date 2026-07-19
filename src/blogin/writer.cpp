#include "writer.h"

#include <cstdint>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>

#include "counters.h"
#include "files.h"
#include "json.h"
#include "sanitizer.h"
#include "value.h"

namespace blogin {
namespace {

constexpr std::string_view manifest_name = ".blogin-manifest.json";
constexpr std::string_view state_name = ".blogin-state.json";

}  // namespace

// Multiplying past 2^64 and keeping the low bits is how FNV mixes, so the
// integer sanitizer's checks are pointed away from this one function rather
// than switched off for the file or the build.
BLOGIN_WRAPS_ON_PURPOSE
std::string content_hash(std::string_view content) {
  // FNV-1a. Not a cryptographic hash and not asked to be: it decides whether a
  // file changed, where a collision costs one unnecessary write.
  std::uint64_t hash = 0xcbf29ce484222325ULL;

  for (const char character : content) {
    hash ^= static_cast<unsigned char>(character);
    hash *= 0x100000001b3ULL;
  }

  return std::format("{:016x}", hash);
}

Writer::Writer(std::filesystem::path root, bool force) : root_(std::move(root)), force_(force) {}

void Writer::load_manifest() {
  const std::string text = files::read_file(root_ / manifest_name);

  if (text.empty()) {
    return;
  }

  const auto parsed = parse_json(text);

  if (!parsed || !parsed->is_object()) {
    return;
  }

  for (const auto& member : parsed->members()) {
    previous_.emplace(member.first, std::string(member.second.as_string()));
  }
}

void Writer::save_manifest() const {
  const std::scoped_lock guard(mutex_);

  Value manifest = Value::object();

  for (const auto& entry : current_) {
    manifest.set(entry.first, Value(entry.second));
  }

  std::error_code error;
  std::filesystem::create_directories(root_, error);

  std::ofstream output(root_ / manifest_name, std::ios::binary | std::ios::trunc);
  const std::string text = to_json(manifest, JsonStyle::compact, true);

  output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::size_t Writer::mark() const {
  const std::scoped_lock guard(mutex_);

  return produced_.size();
}

std::vector<std::string> Writer::since(std::size_t start) const {
  const std::scoped_lock guard(mutex_);

  if (start >= produced_.size()) {
    return {};
  }

  return std::vector<std::string>(produced_.begin() + static_cast<std::ptrdiff_t>(start), produced_.end());
}

std::string Writer::relative(const std::filesystem::path& path) const {
  // Purely lexical on purpose. std::filesystem::relative resolves both paths
  // through realpath, which is a pair of syscalls per file, and every path here
  // was built by joining the root in the first place.
  const std::filesystem::path relative_path = path.lexically_relative(root_);

  if (relative_path.empty() || *relative_path.begin() == "..") {
    return path.generic_string();
  }

  return relative_path.generic_string();
}

void Writer::set_page_filter(std::function<std::string(std::string_view)> filter) {
  page_filter_ = std::move(filter);
}

void Writer::write(const std::filesystem::path& path, std::string_view content) {
  const std::string key = relative(path);

  std::string filtered;

  if (page_filter_ && path.extension() == ".html") {
    filtered = page_filter_(content);
    content = filtered;
  }

  const std::string hash = content_hash(content);

  {
    const std::scoped_lock guard(mutex_);

    if (expected_.insert(key).second) {
      produced_.push_back(key);
    }

    if (const auto earlier = current_.find(key);
        earlier != current_.end() && earlier->second != hash) {
      collisions_.push_back(key);
    }

    current_.insert_or_assign(key, hash);

    if (!force_) {
      const auto known = previous_.find(key);

      if (known != previous_.end() && known->second == hash) {
        std::error_code error;

        if (std::filesystem::exists(path, error)) {
          ++skipped_;

          return;
        }
      }
    }

    ++written_;
    changed_.push_back(path);
  }

  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);

  std::filesystem::path temporary = path;
  temporary += ".tmp";

  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  std::filesystem::rename(temporary, path, error);

  count(Counter::files_written);
}

void Writer::copy(const std::filesystem::path& from, const std::filesystem::path& to) {
  write(to, files::read_file(from));
  count(Counter::files_read);
}

bool Writer::reusable(const std::filesystem::path& path) {
  const std::string key = relative(path);

  const std::scoped_lock guard(mutex_);

  std::error_code error;

  return previous_.contains(key) && std::filesystem::exists(path, error);
}

bool Writer::keep(const std::filesystem::path& path) {
  const std::string key = relative(path);

  const std::scoped_lock guard(mutex_);

  const auto known = previous_.find(key);

  std::error_code error;

  if (known == previous_.end() || !std::filesystem::exists(path, error)) {
    return false;
  }

  if (expected_.insert(key).second) {
    produced_.push_back(key);
  }

  current_.insert_or_assign(key, known->second);
  ++skipped_;

  return true;
}

void Writer::record(const std::filesystem::path& path) {
  const std::scoped_lock guard(mutex_);

  if (const std::string key = relative(path); expected_.insert(key).second) {
    produced_.push_back(key);
  }

  changed_.push_back(path);
}

void Writer::prune() {
  std::error_code error;

  bool removed = false;

  if (!std::filesystem::is_directory(root_, error)) {
    return;
  }

  for (const std::filesystem::path& path : files::all_files(root_)) {
    const std::string key = relative(path);

    // What the build carries between runs is not page output, so pruning
    // leaves both alone.
    if (key == manifest_name || key == state_name || path.filename() == files::keep_file) {
      continue;
    }

    if (expected_.contains(key)) {
      continue;
    }

    std::filesystem::remove(path, error);
    removed = true;
  }

  // A second walk of the whole output tree, so it runs only when something was
  // deleted. Nothing else can empty a directory.
  if (removed) {
    files::prune_empty_directories(root_);
  }
}

}  // namespace blogin
