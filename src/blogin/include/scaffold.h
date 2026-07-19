#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "json.h"

namespace blogin::scaffold {

// The css frameworks a new site can be started with.
const std::vector<std::string>& known_frameworks();

// Every file a new site is made of, by its path relative to the site root.
std::map<std::string, std::string> files(std::string_view framework, std::string_view date);

// Writes a new site into `directory`.
//
// Refuses a directory that already has anything in it, since the alternative is
// overwriting work that is not the scaffold's to overwrite.
std::expected<std::filesystem::path, ParseError> init(const std::filesystem::path& directory,
                                                      std::string_view framework, bool force,
                                                      std::string_view date);

// Writes a stub post and returns where it went.
std::expected<std::filesystem::path, ParseError> new_post(std::string_view title,
                                                          const std::filesystem::path& content,
                                                          std::string_view section, std::string_view date,
                                                          bool force);

// Today, as the date a filename and front matter are written with.
std::string today();

}  // namespace blogin::scaffold
