#pragma once

#include <expected>
#include <filesystem>
#include <string_view>

#include "json.h"
#include "value.h"

namespace blogin::data {

// The data/ tree, keyed by filename with the extension stripped. A
// subdirectory becomes a nested object under its own name.
//
// A directory that is not there yields an empty object, since a site is not
// required to have data files.
std::expected<Value, ParseError> load_tree(const std::filesystem::path& directory);

// Global data merged with each content directory's _data.* along the section
// path. Deeper directories override shallower ones, and both override global.
std::expected<Value, ParseError> resolve(const Value& global, const std::filesystem::path& content,
                                         std::string_view section);

// One file, dispatched on its extension. An extension that is neither JSON nor
// YAML yields nothing rather than an error, so a stray README in data/ is
// ignored rather than fatal.
std::expected<Value, ParseError> load_file(const std::filesystem::path& path);

bool is_data_file(const std::filesystem::path& path);

}  // namespace blogin::data
