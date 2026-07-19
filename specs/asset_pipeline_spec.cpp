#include <atomic>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>

#include "assets.h"
#include "config.h"
#include "files.h"
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

// A site small enough to reason about, carrying one of everything the asset
// pipeline treats differently: a stylesheet that references an image, a script,
// an image, and a static file that must come through untouched.
std::filesystem::path build_site(bool minify, bool fingerprint) {
  const std::filesystem::path root = spec::scratch_directory("assets");

  write(root / "blogin.json", std::string(R"({"title":"Assets","base-url":"https://example.com",)") +
                                R"("minify":)" + (minify ? "true" : "false") + R"(,"fingerprint":)" +
                                (fingerprint ? "true" : "false") + R"(,"search":false})");

  write(root / "layouts" / "base.haml",
        "!!! 5\n%html\n  %head\n    %title= meta-title\n"
        "    %link{rel: 'stylesheet', href: '/assets/css/styles.css'}\n"
        "    %script{src: '/assets/js/app.js'}\n"
        "  %body\n    != yield\n");

  write(root / "layouts" / "show.haml", "%article\n  %h1= title\n  != body\n");
  write(root / "layouts" / "index.haml", "%section\n  %h1= heading\n");

  write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nA post.\n");

  write(root / "assets" / "css" / "styles.css",
        "/* the site stylesheet */\nbody {\n  color: red;\n  background: url(/assets/img/logo.svg);\n}\n");

  write(root / "assets" / "js" / "app.js", "// a note\n\n  const value = 1\n");
  write(root / "assets" / "img" / "logo.svg", "<svg xmlns='http://www.w3.org/2000/svg'></svg>");
  write(root / "static" / "robots-note.txt", "  not an asset  ");

  return root;
}

blogin::BuildOptions options_for(const std::filesystem::path& root) {
  const auto config = blogin::Config::load(root / "blogin.json").value();

  return blogin::BuildOptions::around(root / "content", config);
}

// The one file under `directory` whose name starts with `prefix`, or empty.
std::string named_like(const std::filesystem::path& directory, std::string_view prefix) {
  std::error_code error;

  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (entry.path().filename().string().starts_with(prefix)) {
      return entry.path().filename().string();
    }
  }

  return {};
}

// A minimal 32x32 PNG, written byte for byte so the spec does not depend on an
// image being committed or on a tool being able to make one.
std::string png_32_pixels_wide() {
  static constexpr unsigned char bytes[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfc, 0x18, 0xed,
    0xa3, 0x00, 0x00, 0x00, 0x2a, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0x38, 0x51, 0x61, 0x43,
    0x53, 0xc4, 0x30, 0x6a, 0xc1, 0xa8, 0x05, 0xa3, 0x16, 0x8c, 0x5a, 0x30, 0x6a, 0xc1, 0xa8, 0x05,
    0xa3, 0x16, 0x8c, 0x5a, 0x30, 0x6a, 0xc1, 0xa8, 0x05, 0xa3, 0x16, 0x0c, 0x15, 0x0b, 0x00, 0x58,
    0x4d, 0xf0, 0x4c, 0x04, 0x60, 0xba, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82};

  return std::string(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

}  // namespace

SPEC {
  spec::describe("the asset pipeline", [] {
    spec::context("with minifying and fingerprinting on", [] {
      spec::it("gives a stylesheet a name derived from its content", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(named_like(options.output / "assets" / "css", "styles.")).to_contain(".css");
      });

      spec::it("writes a page pointing at the fingerprinted stylesheet", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::string name = named_like(options.output / "assets" / "css", "styles.");

        expect(read(options.output / "hello" / "index.html")).to_contain("/assets/css/" + name);
      });

      spec::it("leaves no reference to the name the stylesheet no longer has", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "hello" / "index.html")).not_to_contain("/assets/css/styles.css\"");
      });

      // A stylesheet is both an asset that gets a name and a document that
      // references one, so its own bytes have to settle before its name does.
      spec::it("points the stylesheet at the fingerprinted image", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path css =
          options.output / "assets" / "css" / named_like(options.output / "assets" / "css", "styles.");

        expect(read(css)).to_contain(named_like(options.output / "assets" / "img", "logo."));
      });

      spec::it("minifies the stylesheet it writes", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path css =
          options.output / "assets" / "css" / named_like(options.output / "assets" / "css", "styles.");

        expect(read(css)).not_to_contain("/*");
      });

      spec::it("minifies the script it writes", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path js =
          options.output / "assets" / "js" / named_like(options.output / "assets" / "js", "app.");

        expect(read(js)).to_eq("const value = 1");
      });

      // Files at the site root are addressed by name from outside the site.
      spec::it("leaves a static file's name and bytes alone", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "robots-note.txt")).to_eq("  not an asset  ");
      });

      spec::it("writes nothing on a rebuild that changed nothing", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(blogin::build(options)->written).to_eq(std::size_t{0});
      });

      // An unchanged image should cost a stat rather than a read and a hash,
      // which on a site carrying photographs is the difference between a
      // rebuild and a full one.
      spec::it("carries an unchanged asset over without reading it", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        const auto first = blogin::build(options).value();

        spec::aggregate_failures([&] {
          expect(first.assets_reused).to_eq(std::size_t{0});
          expect(blogin::build(options)->assets_reused).to_be_greater_than(std::size_t{0});
        });
      });

      spec::it("reads an asset again once it changes", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::size_t before = blogin::build(options)->assets_reused;

        write(root / "assets" / "img" / "logo.svg", "<svg xmlns='http://www.w3.org/2000/svg' id='x'></svg>");

        expect(blogin::build(options)->assets_reused).to_eq(before - 1);
      });

      // The point of the whole exercise: a changed asset gets a new url, and
      // every page and stylesheet naming it follows.
      spec::it("renames an edited image and follows the reference to it", [] {
        const std::filesystem::path root = build_site(true, true);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::string before = named_like(options.output / "assets" / "img", "logo.");

        write(root / "assets" / "img" / "logo.svg", "<svg xmlns='http://www.w3.org/2000/svg' id='x'></svg>");

        blogin::build(options);

        const std::string after = named_like(options.output / "assets" / "img", "logo.");

        const std::filesystem::path css =
          options.output / "assets" / "css" / named_like(options.output / "assets" / "css", "styles.");

        spec::aggregate_failures([&] {
          expect(after).not_to_eq(before);
          expect(read(css)).to_contain(after);
          expect(std::filesystem::exists(options.output / "assets" / "img" / before)).to_be_false();
        });
      });
    });

    // Resizing needs a program this machine may not have. The example says so
    // rather than passing, since a silent skip claims coverage that never ran.
    spec::context("with responsive images requested", [] {
      spec::it("writes a variant for each width below the original and offers them in a srcset", [] {
        if (blogin::assets::resizer().empty()) {
          spec::pending("no image resizer installed");
        }

        const std::filesystem::path root = build_site(true, true);

        write(root / "blogin.json",
              R"({"title":"Assets","base-url":"https://example.com","minify":true,)"
              R"("fingerprint":true,"search":false,"image-widths":[8,16]})");

        write(root / "assets" / "img" / "photo.png", png_32_pixels_wide());
        write(root / "content" / "hello.md",
              "---\ntitle: Hello\n---\n![a photo](/assets/img/photo.png)\n");

        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path images = options.output / "assets" / "img";
        const std::string page = read(options.output / "hello" / "index.html");

        spec::aggregate_failures([&] {
          expect(named_like(images, "photo-8.")).to_contain(".png");
          expect(named_like(images, "photo-16.")).to_contain(".png");
          expect(page).to_contain("srcset=");
          expect(page).to_contain("8w");
        });
      });

      spec::it("says so when no resizer is installed", [] {
        if (!blogin::assets::resizer().empty()) {
          spec::pending("an image resizer is installed, so nothing is skipped");
        }

        const std::filesystem::path root = build_site(true, true);

        write(root / "blogin.json",
              R"({"title":"Assets","base-url":"https://example.com","minify":true,)"
              R"("fingerprint":true,"search":false,"image-widths":[8,16]})");

        expect(blogin::build(options_for(root))->warnings.size()).to_eq(std::size_t{1});
      });
    });

    spec::context("with both off", [] {
      spec::it("writes the stylesheet under its own name", [] {
        const std::filesystem::path root = build_site(false, false);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(std::filesystem::exists(options.output / "assets" / "css" / "styles.css")).to_be_true();
      });

      spec::it("leaves the stylesheet's bytes as they were written", [] {
        const std::filesystem::path root = build_site(false, false);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "assets" / "css" / "styles.css")).to_contain("/* the site stylesheet */");
      });
    });

    spec::context("with minifying on and fingerprinting off", [] {
      spec::it("minifies under the original name", [] {
        const std::filesystem::path root = build_site(true, false);
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "assets" / "js" / "app.js")).to_eq("const value = 1");
      });
    });
  });
}
