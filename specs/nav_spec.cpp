#include <filesystem>
#include <string>

#include "config.h"
#include "json.h"
#include "nav.h"
#include "support/spec.h"

using blogin::Config;
using spec::expect;

namespace {

std::filesystem::path make_content() {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "blogin-nav-specs" / "content";

  std::filesystem::remove_all(root);

  for (const char* section : {"posts", "guide", "guide/advanced", "notes", ".hidden"}) {
    std::filesystem::create_directories(root / section);
  }

  return root;
}

Config config_from(std::string_view json) {
  return Config::from_value(blogin::parse_json(json).value()).value();
}

}  // namespace

SPEC {
  spec::describe("the navigation tree", [] {
    spec::serial();

    auto content = spec::let([] { return make_content(); });

    spec::context("by default", [=] {
      auto tree = spec::let([=] { return blogin::nav::build_tree(content(), Config{}); });

      spec::it("finds each top-level section", [=] { expect(tree().size()).to_eq(std::size_t{4}); });

      spec::it("nests a subsection under its parent", [=] {
        expect(blogin::nav::find(tree(), "guide")->children.size()).to_eq(std::size_t{1});
      });

      spec::it("gives a section a readable label", [=] {
        expect(blogin::nav::find(tree(), "guide")->label).to_eq("Guide");
      });

      spec::it("gives a subsection its full path", [=] {
        expect(blogin::nav::find(tree(), "guide/advanced") != nullptr).to_be_true();
      });

      spec::it("orders sections by name when nothing says otherwise", [=] {
        expect(tree()[0].name).to_eq(".hidden");
      });

      spec::it("ends a url with a slash when urls are not clean", [=] {
        expect(blogin::nav::find(tree(), "posts")->url).to_eq("/posts/");
      });
    });

    spec::context("with clean urls", [=] {
      spec::it("leaves the trailing slash off", [=] {
        blogin::nav::Options options;
        options.clean_urls = true;

        const auto tree = blogin::nav::build_tree(content(), Config{}, options);

        expect(blogin::nav::find(tree, "posts")->url).to_eq("/posts");
      });
    });

    spec::context("with a url prefix", [=] {
      spec::it("puts the prefix in front", [=] {
        blogin::nav::Options options;
        options.url_prefix = "/fr";

        const auto tree = blogin::nav::build_tree(content(), Config{}, options);

        expect(blogin::nav::find(tree, "posts")->url).to_eq("/fr/posts/");
      });
    });

    spec::context("configured sections", [=] {
      spec::it("uses a configured label", [=] {
        const auto tree = blogin::nav::build_tree(content(),
                                                  config_from(R"({"sections":{"posts":{"label":"Writing"}}})"));

        expect(blogin::nav::find(tree, "posts")->label).to_eq("Writing");
      });

      spec::it("orders by a configured order first", [=] {
        const auto tree = blogin::nav::build_tree(content(),
                                                  config_from(R"({"sections":{"notes":{"order":-1}}})"));

        expect(tree[0].name).to_eq("notes");
      });

      // A section can stay in the tree while keeping out of the menu.
      spec::it("leaves out a section that asked not to appear", [=] {
        const auto tree = blogin::nav::build_tree(content(),
                                                  config_from(R"({"sections":{"posts":{"nav":false}}})"));

        expect(blogin::nav::find(tree, "posts") == nullptr).to_be_true();
      });
    });

    spec::context("marking the current section", [=] {
      auto tree = spec::let([=] { return blogin::nav::build_tree(content(), Config{}); });

      spec::it("marks the section itself", [=] {
        expect(blogin::nav::is_current(*blogin::nav::find(tree(), "guide"), "guide")).to_be_true();
      });

      spec::it("marks an ancestor of the current section", [=] {
        expect(blogin::nav::is_current(*blogin::nav::find(tree(), "guide"), "guide/advanced")).to_be_true();
      });

      // "guides" is not inside "guide", however much the prefix suggests it.
      spec::it("does not mark a section that merely shares a prefix", [=] {
        expect(blogin::nav::is_current(*blogin::nav::find(tree(), "guide"), "guidelines")).to_be_false();
      });

      spec::it("does not mark an unrelated section", [=] {
        expect(blogin::nav::is_current(*blogin::nav::find(tree(), "guide"), "posts")).to_be_false();
      });
    });

    spec::it("finds nothing in an empty tree", [] {
      expect(blogin::nav::build_tree("/nonexistent", Config{}).size()).to_eq(std::size_t{0});
    });
  });
}
