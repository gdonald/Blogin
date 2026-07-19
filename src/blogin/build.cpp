#include "build.h"
#include "files.h"

#include <fstream>
#include <ios>
#include <random>
#include <stdexcept>

#include "counters.h"
#include "html.h"
#include "markdown.h"

namespace blogin {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  if (!input) {
    throw std::runtime_error("cannot read " + path.string());
  }

  std::string content = files::read_file(path);

  count(Counter::files_read);

  return content;
}

// Written to a temporary file and renamed into place. A build killed partway
// then leaves either the previous file or the new one, never a truncated page
// that still looks valid to a web server.
void write_file(const std::filesystem::path& path, std::string_view content) {
  const std::filesystem::path parent = path.parent_path();

  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  std::filesystem::path temporary = path;
  temporary += ".tmp";

  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);

    if (!output) {
      throw std::runtime_error("cannot write " + temporary.string());
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));

    if (!output) {
      throw std::runtime_error("cannot write " + temporary.string());
    }
  }

  std::filesystem::rename(temporary, path);

  count(Counter::files_written);
}

std::string markdown_to_html(std::string markdown) {
  const Source source(std::move(markdown));

  Arena arena;
  const Node* document = parse_markdown(arena, source);

  std::string html = render_html(document);

  count(Counter::posts_parsed);

  return html;
}

void build_page(const std::filesystem::path& markdown_path,
                const std::filesystem::path& template_path,
                const std::filesystem::path& output_path,
                const std::string& title) {
  const CompiledTemplate compiled = CompiledTemplate::compile(read_file(template_path));

  Context context;
  context.set("title", title);
  context.set_body(markdown_to_html(read_file(markdown_path)));

  write_file(output_path, render_template(compiled, context));
}

}  // namespace blogin
