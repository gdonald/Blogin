#pragma once

#include <filesystem>
#include <string_view>
#include <string>
#include <vector>

namespace blogin::files {

// A directory the site owner wants tracked even when the build writes nothing
// into it. It is theirs, not build output, so pruning leaves it.
inline constexpr std::string_view keep_file = ".keep";

// Every file under `root`, in sorted order so a build is reproducible.
//
// Symlinks are not followed, and a directory already visited is skipped, so a
// link pointing at an ancestor cannot hang the walk.
std::vector<std::filesystem::path> all_files(const std::filesystem::path& root);

std::vector<std::filesystem::path> files_with_extension(const std::filesystem::path& root,
                                                        std::string_view extension);

// Directories under `root`, itself included, skipping those whose name begins
// with a dot.
std::vector<std::filesystem::path> descendant_directories(const std::filesystem::path& root);

// The whole of a file, empty when it cannot be read.
//
// Sized once and filled in a single read. Building a string from an
// istreambuf_iterator costs a virtual call per character, which is invisible
// on a page of markdown and is most of the build once a site carries images.
std::string read_file(const std::filesystem::path& path);

void remove_tree(const std::filesystem::path& path);

void prune_empty_directories(const std::filesystem::path& root);

// True when `target` lies strictly inside `base`. Both are resolved first, so
// a path reaching outward through .. or a symlink is caught.
bool within(const std::filesystem::path& target, const std::filesystem::path& base);

}  // namespace blogin::files
