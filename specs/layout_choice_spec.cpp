#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>

#include "config.h"
#include "site.h"
#include "support/spec.h"

using spec::expect;

namespace {

void write(const std::filesystem::path& path, std::string_view body) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

// An essays section configured to use `essay`, and three layouts that each
// say which one rendered the page. `post_front_matter` is spliced into the
// one post's front matter.
std::string render_post(std::string_view post_front_matter, bool with_essay_layout = true) {
  const std::filesystem::path root = spec::scratch_directory("layout-choice");

  write(root / "blogin.json",
        R"({"title":"Layouts","base-url":"https://example.com","search":false,)"
        R"("sections":{"essays":{"layout":"essay"}}})");

  write(root / "layouts" / "show.haml", "%p rendered by show\n");
  write(root / "layouts" / "index.haml", "%p listing\n");
  write(root / "layouts" / "wide.haml", "%p rendered by wide\n");

  if (with_essay_layout) {
    write(root / "layouts" / "essay.haml", "%p rendered by essay\n");
  }

  write(root / "content" / "essays" / "first.md",
        "---\ntitle: First\ndate: 2024-03-07\n" + std::string(post_front_matter) + "---\nAn essay.\n");

  const auto config = blogin::Config::load(root / "blogin.json").value();
  const blogin::BuildOptions options = blogin::BuildOptions::around(root / "content", config);

  blogin::build(options);

  return read(options.output / "essays" / "first" / "index.html");
}

}  // namespace

SPEC {
  spec::describe("choosing a post's layout", [] {
    spec::it("uses the layout its section names in the configuration", [] {
      expect(render_post({})).to_contain("rendered by essay");
    });

    spec::it("prefers the layout the post names in its front matter", [] {
      expect(render_post("layout: wide\n")).to_contain("rendered by wide");
    });

    spec::it("falls back to show when the section's layout does not exist", [] {
      expect(render_post({}, false)).to_contain("rendered by show");
    });
  });
}
