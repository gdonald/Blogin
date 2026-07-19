#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

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

// A site whose theme supplies everything, so what the site overrides can be
// added one file at a time.
std::filesystem::path themed_site() {
  const std::filesystem::path root = spec::scratch_directory("theme");

  write(root / "blogin.json",
        R"({"title":"Themed","base-url":"https://example.com","theme":"plain","search":false,)"
        R"("minify":false,"fingerprint":false})");

  write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nA post.\n");

  const std::filesystem::path theme = root / "themes" / "plain";

  write(theme / "layouts" / "base.haml", "!!! 5\n%html\n  %body\n    %p from the theme\n    != yield\n");
  write(theme / "layouts" / "show.haml", "%article\n  %h1= title\n  != body\n");
  write(theme / "layouts" / "index.haml", "%section\n  %h1= heading\n");
  write(theme / "static" / "robots.txt", "theme robots");
  write(theme / "static" / "theme-only.txt", "only the theme has this");
  write(theme / "assets" / "css" / "style.css", "body{color:blue}");
  write(theme / "assets" / "css" / "theme-only.css", "p{margin:0}");

  return root;
}

blogin::BuildOptions options_for(const std::filesystem::path& root) {
  const auto config = blogin::Config::load(root / "blogin.json").value();

  return blogin::BuildOptions::around(root / "content", config);
}

}  // namespace

SPEC {
  spec::describe("a themed site", [] {
    spec::context("with nothing of its own", [] {
      spec::it("renders through the theme's layout", [] {
        const std::filesystem::path root = themed_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "hello" / "index.html")).to_contain("from the theme");
      });

      spec::it("copies the theme's static files", [] {
        const std::filesystem::path root = themed_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "theme-only.txt")).to_eq("only the theme has this");
      });

      spec::it("copies the theme's assets", [] {
        const std::filesystem::path root = themed_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "assets" / "css" / "theme-only.css")).to_eq("p{margin:0}");
      });
    });

    // A theme you cannot override one file of is a fork waiting to happen.
    spec::context("overriding one file at a time", [] {
      spec::it("prefers the site's own layout", [] {
        const std::filesystem::path root = themed_site();

        write(root / "layouts" / "base.haml", "!!! 5\n%html\n  %body\n    %p from the site\n    != yield\n");

        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "hello" / "index.html")).to_contain("from the site");
      });

      spec::it("leaves the theme's other layouts in use", [] {
        const std::filesystem::path root = themed_site();

        write(root / "layouts" / "base.haml", "!!! 5\n%html\n  %body\n    != yield\n");

        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "hello" / "index.html")).to_contain("<h1>Hello</h1>");
      });

      spec::it("prefers the site's own static file", [] {
        const std::filesystem::path root = themed_site();

        write(root / "static" / "robots.txt", "site robots");

        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "robots.txt")).to_eq("site robots");
      });

      spec::it("prefers the site's own asset", [] {
        const std::filesystem::path root = themed_site();

        write(root / "assets" / "css" / "style.css", "body{color:green}");

        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "assets" / "css" / "style.css")).to_eq("body{color:green}");
      });

      spec::it("keeps the theme's other assets", [] {
        const std::filesystem::path root = themed_site();

        write(root / "assets" / "css" / "style.css", "body{color:green}");

        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "assets" / "css" / "theme-only.css")).to_eq("p{margin:0}");
      });
    });

    // A theme layout can change any page, so a build that reused the last one's
    // work has to notice it changed.
    spec::it("rebuilds when a theme layout changes", [] {
      const std::filesystem::path root = themed_site();
      const blogin::BuildOptions options = options_for(root);

      blogin::build(options);

      write(root / "themes" / "plain" / "layouts" / "base.haml",
            "!!! 5\n%html\n  %body\n    %p edited theme\n    != yield\n");

      blogin::build(options);

      expect(read(options.output / "hello" / "index.html")).to_contain("edited theme");
    });

    // A file the author put in static/ is the author's answer. Generating a
    // second one and letting the order decide which lands is not, and the build
    // refuses to write two different things to one path.
    spec::it("builds without two producers fighting over one path", [] {
      const std::filesystem::path root = themed_site();

      write(root / "static" / "robots.txt", "User-agent: *\nDisallow: /drafts/\n");

      const blogin::BuildOptions options = options_for(root);
      const auto report = blogin::build(options);

      expect(report.has_value() ? std::string{} : report.error().message).to_eq(std::string{});
    });

    spec::it("writes the shipped robots file rather than a generated one", [] {
      const std::filesystem::path root = themed_site();

      write(root / "static" / "robots.txt", "User-agent: *\nDisallow: /drafts/\n");

      const blogin::BuildOptions options = options_for(root);

      blogin::build(options);

      expect(read(options.output / "robots.txt")).to_contain("Disallow: /drafts/");
    });

    spec::it("writes nothing on a rebuild that changed nothing", [] {
      const std::filesystem::path root = themed_site();
      const blogin::BuildOptions options = options_for(root);

      blogin::build(options);

      const auto again = blogin::build(options);

      expect(again.has_value() ? std::string{} : again.error().message).to_eq(std::string{});

      std::vector<std::string> rewritten;

      for (const std::filesystem::path& path : again->changed) {
        rewritten.push_back(std::filesystem::relative(path, options.output).generic_string());
      }

      expect(rewritten).to_eq(std::vector<std::string>{});
    });

    spec::it("names no theme directories when the site names no theme", [] {
      const std::filesystem::path root = spec::scratch_directory("theme");

      write(root / "blogin.json", R"({"title":"Plain","base-url":"https://example.com"})");

      expect(options_for(root).theme_layouts.empty()).to_be_true();
    });
  });
}
