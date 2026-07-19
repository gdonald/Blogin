#include "files.h"
#include <string>
#include <cstdint>
#include <iterator>
#include <ios>
#include <fstream>

#include <algorithm>
#include <set>
#include <system_error>

#include "counters.h"

namespace blogin::files {
namespace {

// A directory identified by what it is rather than by how it was reached, so a
// symlink pointing at an ancestor is recognised as somewhere already visited.
struct DirectoryIdentity {
  std::uintmax_t device = 0;
  std::uintmax_t inode = 0;

  friend auto operator<=>(const DirectoryIdentity&, const DirectoryIdentity&) = default;
};

std::optional<DirectoryIdentity> identify(const std::filesystem::path& directory) {
  std::error_code error;
  const auto status = std::filesystem::status(directory, error);

  if (error || !std::filesystem::is_directory(status)) {
    return std::nullopt;
  }

  // std::filesystem does not expose device and inode portably, so the canonical
  // path stands in: it resolves symlinks, which is what the guard needs.
  const std::filesystem::path canonical = std::filesystem::canonical(directory, error);

  if (error) {
    return std::nullopt;
  }

  const std::size_t hashed = std::filesystem::hash_value(canonical);

  return DirectoryIdentity{hashed, 0};
}

void walk(const std::filesystem::path& directory, std::vector<std::filesystem::path>& found,
          std::set<DirectoryIdentity>& visited) {
  const std::optional<DirectoryIdentity> identity = identify(directory);

  if (!identity || !visited.insert(*identity).second) {
    return;
  }

  count(Counter::directory_walks);

  std::error_code error;
  std::vector<std::filesystem::directory_entry> entries;

  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    entries.push_back(entry);
  }

  if (error) {
    return;
  }

  std::sort(entries.begin(), entries.end(),
            [](const auto& left, const auto& right) { return left.path() < right.path(); });

  for (const auto& entry : entries) {
    if (entry.is_symlink()) {
      continue;
    }

    if (entry.is_directory()) {
      walk(entry.path(), found, visited);
      continue;
    }

    if (entry.is_regular_file()) {
      found.push_back(entry.path());
    }
  }
}

}  // namespace

std::vector<std::filesystem::path> all_files(const std::filesystem::path& root) {
  std::vector<std::filesystem::path> found;
  std::set<DirectoryIdentity> visited;

  walk(root, found, visited);

  return found;
}

std::vector<std::filesystem::path> files_with_extension(const std::filesystem::path& root,
                                                        std::string_view extension) {
  std::vector<std::filesystem::path> matching;

  for (std::filesystem::path& path : all_files(root)) {
    if (path.extension() == extension) {
      matching.push_back(std::move(path));
    }
  }

  return matching;
}

std::vector<std::filesystem::path> descendant_directories(const std::filesystem::path& root) {
  std::vector<std::filesystem::path> found;

  std::error_code error;

  if (!std::filesystem::is_directory(root, error)) {
    return found;
  }

  found.push_back(root);

  std::set<DirectoryIdentity> visited;
  std::vector<std::filesystem::path> queue{root};

  while (!queue.empty()) {
    const std::filesystem::path directory = queue.back();
    queue.pop_back();

    const std::optional<DirectoryIdentity> identity = identify(directory);

    if (!identity || !visited.insert(*identity).second) {
      continue;
    }

    std::vector<std::filesystem::path> children;

    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
      if (!entry.is_directory() || entry.is_symlink()) {
        continue;
      }

      if (entry.path().filename().string().starts_with('.')) {
        continue;
      }

      children.push_back(entry.path());
    }

    std::sort(children.begin(), children.end());

    for (const std::filesystem::path& child : children) {
      found.push_back(child);
      queue.push_back(child);
    }
  }

  std::sort(found.begin(), found.end());

  return found;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  if (!input) {
    return {};
  }

  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(path, error);

  if (error) {
    // Sizing fails for anything that is not a regular file, so this is the
    // branch a directory or a device reaches, and it is where the two standard
    // libraries disagree: libc++ reports a failed read through the stream
    // state, libstdc++ throws from underflow. Opening a directory succeeds on
    // both, so without this a build pointed at one by a typo exits through an
    // uncaught exception on Linux and returns empty on macOS.
    try {
      return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    } catch (const std::ios_base::failure&) {
      return {};
    }
  }

  std::string text;
  text.resize(static_cast<std::size_t>(size));
  input.read(text.data(), static_cast<std::streamsize>(size));
  text.resize(static_cast<std::size_t>(input.gcount()));

  return text;
}

void remove_tree(const std::filesystem::path& path) {
  std::error_code error;

  std::filesystem::remove_all(path, error);
}

void prune_empty_directories(const std::filesystem::path& root) {
  std::error_code error;

  if (!std::filesystem::is_directory(root, error)) {
    return;
  }

  std::vector<std::filesystem::path> children;

  for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
    if (entry.is_directory() && !entry.is_symlink()) {
      children.push_back(entry.path());
    }
  }

  for (const std::filesystem::path& child : children) {
    prune_empty_directories(child);

    if (std::filesystem::is_empty(child, error) && !error) {
      std::filesystem::remove(child, error);
    }
  }
}

bool within(const std::filesystem::path& target, const std::filesystem::path& base) {
  std::error_code error;

  const std::filesystem::path resolved_base = std::filesystem::weakly_canonical(base, error);

  if (error) {
    return false;
  }

  const std::filesystem::path resolved_target = std::filesystem::weakly_canonical(target, error);

  if (error) {
    return false;
  }

  if (resolved_target == resolved_base) {
    return false;
  }

  const auto base_end = resolved_base.end();
  auto base_part = resolved_base.begin();
  auto target_part = resolved_target.begin();

  for (; base_part != base_end; ++base_part, ++target_part) {
    if (target_part == resolved_target.end() || *target_part != *base_part) {
      return false;
    }
  }

  return true;
}

}  // namespace blogin::files
