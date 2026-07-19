#pragma once

#include <filesystem>
#include <string>

#include "template.h"

namespace blogin {

std::string read_file(const std::filesystem::path& path);

void write_file(const std::filesystem::path& path, std::string_view content);

std::string markdown_to_html(std::string markdown);

// One Markdown file through one compiled template to one HTML file: the whole
// path, end to end, on the architecture the rest of the work builds on.
void build_page(const std::filesystem::path& markdown_path,
                const std::filesystem::path& template_path,
                const std::filesystem::path& output_path,
                const std::string& title);

}  // namespace blogin
