#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include "data.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::filesystem::path make_tree(std::string_view name) {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "blogin-data-specs" / name;

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  return root;
}

void write(const std::filesystem::path& path, std::string_view content) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

}  // namespace

SPEC {
  spec::describe("data", [] {
    // Shared scratch directories, and loading counts files read.
    spec::serial();

    spec::context("a tree with a symlink and a nested error", [] {
      spec::it("skips a symlink", [] {
        const std::filesystem::path tree = make_tree("symlink");

        write(tree / "real.json", R"({"a":1})");

        std::error_code ignored;
        std::filesystem::create_symlink(tree / "real.json", tree / "linked.json", ignored);

        const auto loaded = blogin::data::load_tree(tree);

        spec::aggregate_failures([&] {
          expect(loaded.has_value()).to_be_true();
          expect(loaded->contains("linked")).to_be_false();
        });
      });

      // A file that cannot be parsed anywhere in the tree fails the whole load,
      // naming the file, so a broken data file is not silently absent.
      spec::it("reports a broken file inside a subdirectory", [] {
        const std::filesystem::path tree = make_tree("nested-error");

        write(tree / "inner" / "broken.json", "{not json");

        expect(blogin::data::load_tree(tree).has_value()).to_be_false();
      });
    });

    spec::context("loading a tree", [] {
      auto root = spec::let([] {
        const std::filesystem::path tree = make_tree("loading");

        write(tree / "authors.json", R"({"greg":{"name":"Greg"}})");
        write(tree / "menu.yaml", "items:\n  - Home\n  - About\n");
        write(tree / "nested" / "inner.json", R"({"deep":true})");
        write(tree / "README.md", "not data");

        return tree;
      });

      spec::it("keys a file by its name without the extension", [=] {
        expect(blogin::data::load_tree(root()).value().contains("authors")).to_be_true();
      });

      spec::it("reads json", [=] {
        const auto tree = blogin::data::load_tree(root()).value();

        expect(std::string(tree["authors"]["greg"]["name"].as_string())).to_eq("Greg");
      });

      spec::it("reads yaml", [=] {
        const auto tree = blogin::data::load_tree(root()).value();

        expect(tree["menu"]["items"].size()).to_eq(std::size_t{2});
      });

      spec::it("nests a subdirectory under its own name", [=] {
        const auto tree = blogin::data::load_tree(root()).value();

        expect(tree["nested"]["inner"]["deep"].as_boolean()).to_be_true();
      });

      // A stray README in data/ should be ignored, not fatal.
      spec::it("ignores a file that is neither json nor yaml", [=] {
        expect(blogin::data::load_tree(root()).value().contains("README")).to_be_false();
      });

      spec::it("yields an empty object for a directory that is not there", [=] {
        expect(blogin::data::load_tree(root() / "absent").value().empty()).to_be_true();
      });

      spec::it("reports which file failed to parse", [=] {
        write(root() / "broken.json", "{");

        const auto tree = blogin::data::load_tree(root());

        expect(tree.error().message).to_contain("broken.json");
      });
    });

    spec::context("resolving along a section path", [] {
      auto content = spec::let([] {
        const std::filesystem::path tree = make_tree("resolving");

        write(tree / "_data.json", R"({"site":"root","shared":{"a":1}})");
        write(tree / "posts" / "_data.json", R"({"site":"posts","shared":{"b":2}})");
        write(tree / "posts" / "deep" / "_data.yaml", "site: deep\n");

        return tree;
      });

      auto global = spec::let([] {
        blogin::Value value = blogin::Value::object();
        value.set("from-global", blogin::Value(true));
        value.set("site", blogin::Value("global"));

        return value;
      });

      spec::it("keeps global values a section does not override", [=] {
        const auto resolved = blogin::data::resolve(global(), content(), "posts").value();

        expect(resolved["from-global"].as_boolean()).to_be_true();
      });

      spec::it("lets the content root override global", [=] {
        const auto resolved = blogin::data::resolve(global(), content(), "").value();

        expect(std::string(resolved["site"].as_string())).to_eq("root");
      });

      spec::it("lets a section override the content root", [=] {
        const auto resolved = blogin::data::resolve(global(), content(), "posts").value();

        expect(std::string(resolved["site"].as_string())).to_eq("posts");
      });

      spec::it("lets a deeper section override a shallower one", [=] {
        const auto resolved = blogin::data::resolve(global(), content(), "posts/deep").value();

        expect(std::string(resolved["site"].as_string())).to_eq("deep");
      });

      spec::it("merges nested objects rather than replacing them", [=] {
        const auto resolved = blogin::data::resolve(global(), content(), "posts").value();

        spec::aggregate_failures([&] {
          expect(resolved["shared"]["a"].as_integer()).to_eq(std::int64_t{1});
          expect(resolved["shared"]["b"].as_integer()).to_eq(std::int64_t{2});
        });
      });

      spec::it("reads yaml data files too", [=] {
        const auto resolved = blogin::data::resolve(global(), content(), "posts/deep").value();

        expect(resolved.contains("site")).to_be_true();
      });

      spec::it("ignores a section with no data file", [=] {
        const auto resolved = blogin::data::resolve(global(), content(), "absent").value();

        expect(std::string(resolved["site"].as_string())).to_eq("root");
      });
    });

    spec::context("recognising data files", [] {
      spec::it("accepts json", [] { expect(blogin::data::is_data_file("a.json")).to_be_true(); });

      spec::it("accepts yaml", [] { expect(blogin::data::is_data_file("a.yaml")).to_be_true(); });

      spec::it("accepts yml", [] { expect(blogin::data::is_data_file("a.yml")).to_be_true(); });

      spec::it("accepts an uppercase extension", [] {
        expect(blogin::data::is_data_file("a.JSON")).to_be_true();
      });

      spec::it("rejects markdown", [] { expect(blogin::data::is_data_file("a.md")).to_be_false(); });

      spec::it("rejects a file with no extension", [] {
        expect(blogin::data::is_data_file("README")).to_be_false();
      });
    });
  });
}
