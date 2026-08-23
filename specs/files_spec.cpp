#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include "counters.h"
#include "files.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::filesystem::path make_tree(std::string_view name) {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "blogin-files-specs" / name;

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  return root;
}

void touch(const std::filesystem::path& path, std::string_view content = "x") {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

}  // namespace

SPEC {
  spec::describe("files", [] {
    spec::context("a tree that is not there", [] {
      spec::it("lists nothing for a directory that does not exist", [] {
        expect(blogin::files::all_files(make_tree("absent") / "gone").size()).to_eq(std::size_t{0});
      });

      spec::it("finds no descendants of a directory that does not exist", [] {
        expect(blogin::files::descendant_directories(make_tree("absent-dirs") / "gone").size())
          .to_eq(std::size_t{0});
      });
    });

    // The scratch tree is shared, and the walk touches the global directory
    // counter.
    spec::serial();

    // A path that exists and is not a directory has nothing under it either,
    // and asking is how a caller finds that out.
    spec::context("a path that is a file", [] {
      spec::it("lists nothing under it", [] {
        const std::filesystem::path root = make_tree("file-not-directory");

        touch(root / "page.md");

        expect(blogin::files::all_files(root / "page.md").size()).to_eq(std::size_t{0});
      });

      spec::it("finds no descendants of it", [] {
        const std::filesystem::path root = make_tree("file-not-directory-dirs");

        touch(root / "page.md");

        expect(blogin::files::descendant_directories(root / "page.md").size()).to_eq(std::size_t{0});
      });

      // Answering with nothing is not the same as looking. A file is rejected
      // before the walk counts a directory it was never going to read.
      spec::it("does not count a directory walk for it", [] {
        const std::filesystem::path root = make_tree("file-not-directory-walks");

        touch(root / "page.md");

        blogin::reset_counters();
        blogin::files::all_files(root / "page.md");

        expect(blogin::counter_value(blogin::Counter::directory_walks)).to_eq(std::uint64_t{0});
      });
    });

    // A symlink pointing back up the tree would otherwise be walked forever.
    spec::context("a directory reachable twice", [] {
      spec::it("walks it once", [] {
        const std::filesystem::path root = make_tree("loop");

        touch(root / "inner" / "a.md");

        std::error_code ignored;
        std::filesystem::create_directory_symlink(root, root / "inner" / "back", ignored);

        expect(blogin::files::descendant_directories(root).size()).to_be_less_than(std::size_t{8});
      });
    });

    spec::context("listing every file", [] {
      auto root = spec::let([] {
        const std::filesystem::path tree = make_tree("listing");

        touch(tree / "b.md");
        touch(tree / "a.md");
        touch(tree / "nested" / "c.md");
        touch(tree / "nested" / "deeper" / "d.md");

        return tree;
      });

      spec::it("finds files at every depth", [=] {
        expect(blogin::files::all_files(root()).size()).to_eq(std::size_t{4});
      });

      // A build that listed files in whatever order the filesystem returned
      // would produce different output run to run.
      spec::it("returns them sorted", [=] {
        const auto found = blogin::files::all_files(root());

        expect(found[0].filename().string()).to_eq("a.md");
      });

      spec::it("returns nothing for a directory that is not there", [=] {
        expect(blogin::files::all_files(root() / "absent").size()).to_eq(std::size_t{0});
      });

      spec::it("filters by extension", [=] {
        touch(root() / "notes.txt");

        expect(blogin::files::files_with_extension(root(), ".md").size()).to_eq(std::size_t{4});
      });
    });

    spec::context("a symlink pointing at an ancestor", [] {
      auto root = spec::let([] {
        const std::filesystem::path tree = make_tree("symlink-loop");

        touch(tree / "content" / "post.md");

        std::error_code error;
        std::filesystem::create_directory_symlink(tree, tree / "content" / "loop", error);

        return tree;
      });

      // Without a guard this walk never returns.
      spec::it("does not hang the walk", [=] {
        expect(blogin::files::all_files(root()).size()).to_eq(std::size_t{1});
      });
    });

    spec::context("descendant directories", [] {
      auto root = spec::let([] {
        const std::filesystem::path tree = make_tree("directories");

        touch(tree / "one" / "file.md");
        touch(tree / "one" / "two" / "file.md");
        touch(tree / ".hidden" / "file.md");

        return tree;
      });

      spec::it("includes the root itself", [=] {
        const auto found = blogin::files::descendant_directories(root());

        expect(found[0] == root()).to_be_true();
      });

      spec::it("skips directories whose name begins with a dot", [=] {
        const auto found = blogin::files::descendant_directories(root());

        expect(found.size()).to_eq(std::size_t{3});
      });

      spec::it("returns nothing for a directory that is not there", [=] {
        expect(blogin::files::descendant_directories(root() / "absent").size()).to_eq(std::size_t{0});
      });
    });

    spec::context("pruning empty directories", [] {
      auto root = spec::let([] {
        const std::filesystem::path tree = make_tree("pruning");

        std::filesystem::create_directories(tree / "empty" / "deeper");
        touch(tree / "kept" / "file.md");

        return tree;
      });

      spec::it("removes a directory with nothing in it", [=] {
        blogin::files::prune_empty_directories(root());

        expect(std::filesystem::exists(root() / "empty")).to_be_false();
      });

      spec::it("leaves a directory that holds a file", [=] {
        blogin::files::prune_empty_directories(root());

        expect(std::filesystem::exists(root() / "kept")).to_be_true();
      });

      // A configured output path that names a file, not a directory,
      // reaches here, and removing what the site owner pointed at would be the
      // wrong answer to a typo.
      spec::it("leaves a path that is a file rather than a directory", [=] {
        touch(root() / "not-a-directory");

        blogin::files::prune_empty_directories(root() / "not-a-directory");

        expect(std::filesystem::exists(root() / "not-a-directory")).to_be_true();
      });
    });

    spec::context("reading a file", [] {
      spec::it("gives back what the file holds", [] {
        const std::filesystem::path tree = make_tree("reading");

        touch(tree / "post.md", "# Title\nbody\n");

        expect(blogin::files::read_file(tree / "post.md")).to_eq("# Title\nbody\n");
      });

      spec::it("gives back nothing for a path that is not there", [] {
        const std::filesystem::path tree = make_tree("reading-absent");

        expect(blogin::files::read_file(tree / "absent.md")).to_eq("");
      });

      spec::it("gives back nothing for a directory", [] {
        const std::filesystem::path tree = make_tree("reading-directory");

        expect(blogin::files::read_file(tree)).to_eq("");
      });

      // Sizing the file first and reading it in one go is the fast path. A path
      // whose size cannot be measured has to fall back to streaming instead of
      // resize a string to whatever the failed call left behind.
      spec::it("reads a path whose size cannot be measured", [] {
        const std::filesystem::path device = "/dev/null";

        if (!std::filesystem::exists(device)) {
          spec::pending("no /dev/null on this platform");
        }

        expect(blogin::files::read_file(device)).to_eq("");
      });
    });

    spec::context("removing a tree", [] {
      spec::it("removes everything under it", [] {
        const std::filesystem::path tree = make_tree("removal");

        touch(tree / "one" / "two" / "file.md");

        blogin::files::remove_tree(tree);

        expect(std::filesystem::exists(tree)).to_be_false();
      });

      spec::it("says nothing about a path that is not there", [] {
        const std::filesystem::path tree = make_tree("removal-absent");

        blogin::files::remove_tree(tree / "absent");

        expect(std::filesystem::exists(tree)).to_be_true();
      });
    });

    // This is the guard behind refusing to clean a directory outside the site
    // root, so it is covered case by case.
    spec::context("containment", [] {
      auto root = spec::let([] { return make_tree("containment"); });

      spec::it("accepts a path inside", [=] {
        std::filesystem::create_directories(root() / "public");

        expect(blogin::files::within(root() / "public", root())).to_be_true();
      });

      spec::it("accepts a path nested deeply inside", [=] {
        std::filesystem::create_directories(root() / "a" / "b" / "c");

        expect(blogin::files::within(root() / "a" / "b" / "c", root())).to_be_true();
      });

      spec::it("rejects the root itself", [=] {
        expect(blogin::files::within(root(), root())).to_be_false();
      });

      spec::it("rejects a sibling", [=] {
        expect(blogin::files::within(root().parent_path() / "elsewhere", root())).to_be_false();
      });

      spec::it("rejects a parent", [=] {
        expect(blogin::files::within(root().parent_path(), root())).to_be_false();
      });

      spec::it("rejects a path that climbs out and back", [=] {
        expect(blogin::files::within(root() / ".." / "elsewhere", root())).to_be_false();
      });

      spec::it("rejects a name that merely shares a prefix", [=] {
        expect(blogin::files::within(root().string() + "-other", root())).to_be_false();
      });
    });
  });
}
