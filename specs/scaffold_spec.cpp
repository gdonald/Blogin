#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>

#include "config.h"
#include "scaffold.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

// A fixed date, so what the scaffold writes does not depend on the day it runs.
constexpr std::string_view fixed_date = "2026-03-04";

std::filesystem::path scaffolded(std::string_view framework = "none") {
  const std::filesystem::path root = spec::scratch_directory("scaffold");

  blogin::scaffold::init(root, framework, false, fixed_date);

  return root;
}

}  // namespace

SPEC {
  spec::describe("scaffolding a site", [] {
    spec::context("what it writes", [] {
      spec::it("writes a configuration file", [] {
        expect(std::filesystem::exists(scaffolded() / "blogin.json")).to_be_true();
      });

      spec::it("writes a configuration the build accepts without complaint", [] {
        const auto config = blogin::Config::load(scaffolded() / "blogin.json");

        spec::aggregate_failures([&] {
          expect(config.has_value()).to_be_true();
          expect(config->unknown_keys).to_eq(std::vector<std::string>{});
        });
      });

      // There is no plugin system here, so a key the build would reject has no
      // business in the file it writes for you.
      spec::it("leaves out the plugins key", [] {
        expect(read(scaffolded() / "blogin.json")).not_to_contain("plugins");
      });

      spec::it("writes a starter post dated today", [] {
        expect(std::filesystem::exists(scaffolded() / "content" / "posts" /
                                       (std::string(fixed_date) + "-hello-world.md")))
          .to_be_true();
      });

      spec::it("writes the base layout", [] {
        expect(std::filesystem::exists(scaffolded() / "layouts" / "base.haml")).to_be_true();
      });

      spec::it("writes every partial the base layout renders", [] {
        const std::filesystem::path root = scaffolded();

        spec::aggregate_failures([&] {
          for (const std::string_view name :
               {"_header.haml", "_nav.haml", "_nav-item.haml", "_footer.haml", "_search.haml",
                "_entry.haml"}) {
            expect(std::filesystem::exists(root / "layouts" / name)).to_be_true();
          }
        });
      });

      spec::it("writes a stylesheet", [] {
        expect(read(scaffolded() / "assets" / "css" / "style.css")).to_contain("--blogin-bg");
      });

      // Somewhere obvious to put them, rather than a guess about where they go.
      spec::it("makes a place for scripts and images", [] {
        const std::filesystem::path root = scaffolded();

        spec::aggregate_failures([&] {
          expect(std::filesystem::is_directory(root / "assets" / "js")).to_be_true();
          expect(std::filesystem::is_directory(root / "assets" / "img")).to_be_true();
        });
      });
    });

    spec::context("with bootstrap", [] {
      spec::it("records the framework in the configuration", [] {
        expect(read(scaffolded("bootstrap5") / "blogin.json")).to_contain("\"bootstrap5\"");
      });

      spec::it("writes a base layout using bootstrap's classes", [] {
        expect(read(scaffolded("bootstrap5") / "layouts" / "base.haml")).to_contain("d-flex");
      });

      // Bootstrap brings its own, so the plain one would only be dead weight.
      spec::it("writes no stylesheet of its own", [] {
        expect(read(scaffolded("bootstrap5") / "assets" / "css" / "style.css")).to_eq("");
      });
    });

    spec::context("refusing", [] {
      spec::it("refuses a framework it does not know", [] {
        const std::filesystem::path root = spec::scratch_directory("scaffold");

        expect(blogin::scaffold::init(root, "tailwind", false, fixed_date).error().message)
          .to_contain("unknown framework 'tailwind'");
      });

      spec::it("names the frameworks it does know", [] {
        const std::filesystem::path root = spec::scratch_directory("scaffold");

        expect(blogin::scaffold::init(root, "tailwind", false, fixed_date).error().message)
          .to_contain("none, bootstrap5");
      });

      // Whatever is already there belongs to someone else.
      spec::it("refuses a directory that is not empty", [] {
        const std::filesystem::path root = scaffolded();

        expect(blogin::scaffold::init(root, "none", false, fixed_date).error().message)
          .to_contain("is not empty");
      });

      spec::it("names what is in the way", [] {
        const std::filesystem::path root = scaffolded();

        expect(blogin::scaffold::init(root, "none", false, fixed_date).error().message)
          .to_contain("blogin.json");
      });

      spec::it("writes into a directory that is not empty when told to", [] {
        const std::filesystem::path root = scaffolded();

        expect(blogin::scaffold::init(root, "none", true, fixed_date).has_value()).to_be_true();
      });
    });
  });

  spec::describe("scaffolding a post", [] {
    spec::it("names the file for the date and the title", [] {
      const std::filesystem::path root = spec::scratch_directory("post");

      const auto made = blogin::scaffold::new_post("My First Post", root, "", fixed_date, false);

      expect(made->filename().string()).to_eq("2026-03-04-my-first-post.md");
    });

    spec::it("puts it in the section it was given", [] {
      const std::filesystem::path root = spec::scratch_directory("post");

      const auto made = blogin::scaffold::new_post("A Note", root, "notes", fixed_date, false);

      expect(made->parent_path().filename().string()).to_eq("notes");
    });

    spec::it("writes front matter carrying the title", [] {
      const std::filesystem::path root = spec::scratch_directory("post");

      const auto made = blogin::scaffold::new_post("A Note", root, "", fixed_date, false);

      expect(read(*made)).to_contain("title: \"A Note\"");
    });

    spec::it("writes front matter carrying the date", [] {
      const std::filesystem::path root = spec::scratch_directory("post");

      const auto made = blogin::scaffold::new_post("A Note", root, "", fixed_date, false);

      expect(read(*made)).to_contain("date: 2026-03-04");
    });

    spec::it("refuses to write over a post that already exists", [] {
      const std::filesystem::path root = spec::scratch_directory("post");

      blogin::scaffold::new_post("A Note", root, "", fixed_date, false);

      expect(blogin::scaffold::new_post("A Note", root, "", fixed_date, false).error().message)
        .to_contain("already exists");
    });

    spec::it("writes over a post that already exists when told to", [] {
      const std::filesystem::path root = spec::scratch_directory("post");

      blogin::scaffold::new_post("A Note", root, "", fixed_date, false);

      expect(blogin::scaffold::new_post("A Note", root, "", fixed_date, true).has_value()).to_be_true();
    });
  });
}
