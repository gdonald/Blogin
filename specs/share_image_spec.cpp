#include <algorithm>
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

std::string big_endian_32(unsigned value) {
  std::string out;

  for (int shift = 24; shift >= 0; shift -= 8) {
    out += static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU);
  }

  return out;
}

// The bytes a size is read from, and nothing a viewer would need.
std::string png_header(unsigned width, unsigned height) {
  return std::string("\x89PNG\r\n\x1A\n", 8) + big_endian_32(13) + "IHDR" + big_endian_32(width) +
         big_endian_32(height);
}

// Two posts, so a spec can tell the page that names an image from the one that
// does not. `config` is spliced into blogin.json.
std::filesystem::path make_site(std::string_view config, std::string_view first_front_matter = {}) {
  const std::filesystem::path root = spec::scratch_directory("share-image");

  write(root / "blogin.json",
        std::string(R"({"title":"Shared","base-url":"https://example.com","search":false)") + std::string(config) +
          "}");

  write(root / "layouts" / "base.haml", "!!! 5\n%html\n  %head\n    != head-meta\n  %body\n    != yield\n");
  write(root / "layouts" / "show.haml", "%article\n  %h1= title\n  != body\n");
  write(root / "layouts" / "index.haml", "%section\n  %h1= heading\n");

  write(root / "content" / "posts" / "first.md",
        "---\ntitle: First\ndate: 2024-03-07\n" + std::string(first_front_matter) + "---\nThe first post.\n");
  write(root / "content" / "posts" / "second.md", "---\ntitle: Second\ndate: 2024-03-08\n---\nThe second post.\n");

  write(root / "assets" / "images" / "default.png", png_header(1200, 627));
  write(root / "assets" / "images" / "first.png", png_header(800, 418));

  return root;
}

blogin::BuildOptions options_for(const std::filesystem::path& root) {
  const auto config = blogin::Config::load(root / "blogin.json").value();

  return blogin::BuildOptions::around(root / "content", config);
}

std::string first_page(const blogin::BuildOptions& options) {
  return read(options.output / "posts" / "first" / "index.html");
}

std::string second_page(const blogin::BuildOptions& options) {
  return read(options.output / "posts" / "second" / "index.html");
}

bool warned_about(const blogin::BuildReport& report, std::string_view text) {
  return std::ranges::any_of(report.warnings,
                             [&](const std::string& warning) { return warning.find(text) != std::string::npos; });
}

}  // namespace

SPEC {
  spec::describe("share images", [] {
    spec::context("with a site default and a post that names its own", [] {
      auto options = spec::let([] {
        const blogin::BuildOptions built =
          options_for(make_site(R"(,"image":"/assets/images/default.png")", "image: /assets/images/first.png\n"));

        blogin::build(built);

        return built;
      });

      spec::it("gives the post the image it names", [=] {
        expect(first_page(options()))
          .to_contain(R"(<meta property="og:image" content="https://example.com/assets/images/first.png"/>)");
      });

      spec::it("reads the size of the post's image from the file", [=] {
        spec::aggregate_failures([&] {
          expect(first_page(options())).to_contain(R"(<meta property="og:image:width" content="800"/>)");
          expect(first_page(options())).to_contain(R"(<meta property="og:image:height" content="418"/>)");
        });
      });

      spec::it("gives a post that names none the site default", [=] {
        expect(second_page(options())).to_contain(R"(content="https://example.com/assets/images/default.png")");
      });

      spec::it("gives a listing the site default", [=] {
        expect(read(options().output / "posts" / "index.html"))
          .to_contain(R"(content="https://example.com/assets/images/default.png")");
      });

      spec::it("writes the large twitter card", [=] {
        expect(second_page(options())).to_contain(R"(content="summary_large_image")");
      });
    });

    spec::context("with no image anywhere", [] {
      spec::it("writes no og:image", [] {
        const blogin::BuildOptions options = options_for(make_site({}));

        blogin::build(options);

        expect(second_page(options)).not_to_contain("og:image");
      });
    });

    spec::context("with fingerprinting on", [] {
      spec::it("points at the fingerprinted name", [] {
        const blogin::BuildOptions options =
          options_for(make_site(R"(,"fingerprint":true,"image":"/assets/images/default.png")"));

        blogin::build(options);

        expect(second_page(options)).to_contain(R"(content="https://example.com/assets/images/default.)");
      });

      spec::it("leaves no reference to the name the image no longer has", [] {
        const blogin::BuildOptions options =
          options_for(make_site(R"(,"fingerprint":true,"image":"/assets/images/default.png")"));

        blogin::build(options);

        expect(second_page(options)).not_to_contain("/assets/images/default.png");
      });
    });

    spec::context("finding the file", [] {
      spec::it("reads an image from the static tree", [] {
        const std::filesystem::path root = make_site(R"(,"image":"/share.png")");
        write(root / "static" / "share.png", png_header(1200, 630));

        const blogin::BuildOptions options = options_for(root);
        blogin::build(options);

        expect(second_page(options)).to_contain(R"(<meta property="og:image:height" content="630"/>)");
      });

      spec::it("reads a path written without a leading slash from the site root", [] {
        const blogin::BuildOptions options = options_for(make_site(R"(,"image":"assets/images/default.png")"));

        blogin::build(options);

        expect(second_page(options)).to_contain(R"(content="https://example.com/assets/images/default.png")");
      });

      spec::it("reads an image from the theme's assets when the site has none by that name", [] {
        const std::filesystem::path root = make_site(R"(,"image":"/assets/images/theme.png")");
        write(root / "theme-assets" / "images" / "theme.png", png_header(1000, 523));

        blogin::BuildOptions options = options_for(root);
        options.theme_assets = root / "theme-assets";
        blogin::build(options);

        expect(second_page(options)).to_contain(R"(<meta property="og:image:width" content="1000"/>)");
      });

      spec::it("writes an image on another host as it is", [] {
        const blogin::BuildOptions options =
          options_for(make_site(R"(,"image":"https://cdn.example.net/share.png")"));

        blogin::build(options);

        expect(second_page(options)).to_contain(R"(content="https://cdn.example.net/share.png")");
      });

      spec::it("writes an image on another host by plain http as it is", [] {
        const blogin::BuildOptions options = options_for(make_site(R"(,"image":"http://cdn.example.net/share.png")"));

        blogin::build(options);

        expect(second_page(options)).to_contain(R"(content="http://cdn.example.net/share.png")");
      });
    });

    spec::context("with an image that is not there", [] {
      auto built = spec::let([] {
        const blogin::BuildOptions options = options_for(make_site(R"(,"image":"/assets/images/missing.png")"));

        return std::make_pair(options, blogin::build(options).value());
      });

      spec::it("says so", [=] {
        expect(warned_about(built().second, "share image '/assets/images/missing.png' is not in assets or static"))
          .to_be_true();
      });

      spec::it("says so once however many pages name it", [=] {
        const auto count = std::ranges::count_if(built().second.warnings, [](const std::string& warning) {
          return warning.find("missing.png") != std::string::npos;
        });

        expect(count).to_eq(std::ptrdiff_t{1});
      });

      spec::it("still writes the image", [=] {
        expect(second_page(built().first)).to_contain(R"(content="https://example.com/assets/images/missing.png")");
      });

      spec::it("leaves out the size", [=] { expect(second_page(built().first)).not_to_contain("og:image:width"); });
    });

    spec::context("with an image whose size cannot be read", [] {
      auto built = spec::let([] {
        const std::filesystem::path root = make_site(R"(,"image":"/assets/images/share.svg")");
        write(root / "assets" / "images" / "share.svg", "<svg xmlns='http://www.w3.org/2000/svg'/>");

        const blogin::BuildOptions options = options_for(root);

        return std::make_pair(options, blogin::build(options).value());
      });

      spec::it("says so", [=] {
        expect(warned_about(built().second, "share image '/assets/images/share.svg' has no size")).to_be_true();
      });

      spec::it("leaves out the size", [=] { expect(second_page(built().first)).not_to_contain("og:image:width"); });
    });

    spec::context("rebuilding", [] {
      spec::it("renders nothing when nothing changed", [] {
        const blogin::BuildOptions options =
          options_for(make_site(R"(,"image":"/assets/images/default.png")", "image: /assets/images/first.png\n"));

        blogin::build(options);

        expect(blogin::build(options)->rendered).to_eq(std::size_t{0});
      });

      // The post's own file did not change, so only the size recorded against
      // its page says it is out of date.
      spec::it("renders a post again when the image it names changes size", [] {
        const std::filesystem::path root =
          make_site(R"(,"image":"/assets/images/default.png")", "image: /assets/images/first.png\n");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        write(root / "assets" / "images" / "first.png", png_header(1200, 627));

        const blogin::BuildReport again = blogin::build(options).value();

        spec::aggregate_failures([&] {
          expect(again.rendered).to_eq(std::size_t{1});
          expect(first_page(options)).to_contain(R"(<meta property="og:image:width" content="1200"/>)");
        });
      });

      spec::it("renders every page again when the site default changes size", [] {
        const std::filesystem::path root = make_site(R"(,"image":"/assets/images/default.png")");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        write(root / "assets" / "images" / "default.png", png_header(1200, 630));
        blogin::build(options);

        spec::aggregate_failures([&] {
          expect(second_page(options)).to_contain(R"(<meta property="og:image:height" content="630"/>)");
          expect(read(options.output / "posts" / "index.html"))
            .to_contain(R"(<meta property="og:image:height" content="630"/>)");
        });
      });
    });
  });
}
