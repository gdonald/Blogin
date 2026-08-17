#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "files.h"
#include "support/spec.h"
#include "writer.h"

using blogin::Writer;
using spec::expect;

namespace {

std::filesystem::path scratch() {
  const std::filesystem::path root = spec::scratch_directory("writer");

  std::filesystem::create_directories(root);

  return root;
}

std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& path, std::string_view body) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

}  // namespace

// What lands in the output tree, and what is left alone. The build decides what
// to render, and this decides what reaches disk.
SPEC {
  spec::describe("the writer", [] {
    spec::context("writing", [] {
      spec::it("writes a file", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.write(root / "page.html", "body");

        expect(read(root / "page.html")).to_eq("body");
      });

      spec::it("creates the directories a path needs", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.write(root / "deep" / "down" / "page.html", "body");

        expect(std::filesystem::exists(root / "deep" / "down" / "page.html")).to_be_true();
      });

      spec::it("counts what it wrote", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.write(root / "page.html", "body");

        expect(writer.written()).to_eq(std::size_t{1});
      });

      spec::it("copies a file", [] {
        const std::filesystem::path root = scratch();

        write_file(root / "source.txt", "copied");

        Writer writer(root);
        writer.copy(root / "source.txt", root / "out" / "target.txt");

        expect(read(root / "out" / "target.txt")).to_eq("copied");
      });

      // Every page passes through it, so a new kind of page cannot forget to
      // rewrite its asset urls.
      spec::it("puts a page through the filter it was given", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.set_page_filter([](std::string_view page) { return std::string(page) + "!"; });
        writer.write(root / "page.html", "body");

        expect(read(root / "page.html")).to_eq("body!");
      });

      spec::it("leaves anything that is not a page out of the filter", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.set_page_filter([](std::string_view page) { return std::string(page) + "!"; });
        writer.write(root / "feed.xml", "body");

        expect(read(root / "feed.xml")).to_eq("body");
      });
    });

    // Two results at one path is an accident of ordering, so it is reported
    // and not resolved. Writing the same bytes twice is harmless.
    spec::context("collisions", [] {
      spec::it("reports a path written twice with different content", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.write(root / "page.html", "one");
        writer.write(root / "page.html", "two");

        expect(writer.collisions()).to_eq(std::vector<std::string>{"page.html"});
      });

      spec::it("says nothing about a path written twice with the same content", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.write(root / "page.html", "one");
        writer.write(root / "page.html", "one");

        expect(writer.collisions()).to_eq(std::vector<std::string>{});
      });
    });

    spec::context("carrying a manifest between builds", [] {
      spec::it("writes nothing the second time when nothing changed", [] {
        const std::filesystem::path root = scratch();

        Writer first(root);
        first.load_manifest();
        first.write(root / "page.html", "body");
        first.save_manifest();

        Writer second(root);
        second.load_manifest();
        second.write(root / "page.html", "body");

        expect(second.written()).to_eq(std::size_t{0});
      });

      spec::it("writes again when the content changed", [] {
        const std::filesystem::path root = scratch();

        Writer first(root);
        first.load_manifest();
        first.write(root / "page.html", "body");
        first.save_manifest();

        Writer second(root);
        second.load_manifest();
        second.write(root / "page.html", "different");

        expect(second.written()).to_eq(std::size_t{1});
      });

      // A manifest nobody can read means everything is written this time, not
      // that the build fails.
      spec::it("ignores a manifest that is not json", [] {
        const std::filesystem::path root = scratch();

        write_file(root / ".blogin-manifest.json", "{not json");

        Writer writer(root);
        writer.load_manifest();
        writer.write(root / "page.html", "body");

        expect(writer.written()).to_eq(std::size_t{1});
      });

      spec::it("ignores a manifest that is not an object", [] {
        const std::filesystem::path root = scratch();

        write_file(root / ".blogin-manifest.json", "[1, 2]");

        Writer writer(root);
        writer.load_manifest();
        writer.write(root / "page.html", "body");

        expect(writer.written()).to_eq(std::size_t{1});
      });

      spec::it("writes everything when it is forced to", [] {
        const std::filesystem::path root = scratch();

        Writer first(root);
        first.load_manifest();
        first.write(root / "page.html", "body");
        first.save_manifest();

        Writer forced(root, true);
        forced.load_manifest();
        forced.write(root / "page.html", "body");

        expect(forced.written()).to_eq(std::size_t{1});
      });
    });

    spec::context("keeping a file the build did not produce again", [] {
      spec::it("keeps one the last build wrote", [] {
        const std::filesystem::path root = scratch();

        Writer first(root);
        first.load_manifest();
        first.write(root / "page.html", "body");
        first.save_manifest();

        Writer second(root);
        second.load_manifest();

        expect(second.keep(root / "page.html")).to_be_true();
      });

      spec::it("counts it as skipped rather than written", [] {
        const std::filesystem::path root = scratch();

        Writer first(root);
        first.load_manifest();
        first.write(root / "page.html", "body");
        first.save_manifest();

        Writer second(root);
        second.load_manifest();
        second.keep(root / "page.html");

        expect(second.skipped()).to_eq(std::size_t{1});
      });

      spec::it("refuses to keep one no manifest knows about", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);

        expect(writer.keep(root / "page.html")).to_be_false();
      });

      spec::it("refuses to keep one that is no longer on disk", [] {
        const std::filesystem::path root = scratch();

        Writer first(root);
        first.load_manifest();
        first.write(root / "page.html", "body");
        first.save_manifest();

        std::filesystem::remove(root / "page.html");

        Writer second(root);
        second.load_manifest();

        expect(second.keep(root / "page.html")).to_be_false();
      });

      // Asked before the build knows whether it will render the page.
      spec::it("says in advance whether keeping would work", [] {
        const std::filesystem::path root = scratch();

        Writer first(root);
        first.load_manifest();
        first.write(root / "page.html", "body");
        first.save_manifest();

        Writer second(root);
        second.load_manifest();

        expect(second.reusable(root / "page.html")).to_be_true();
      });

      spec::it("says in advance when keeping would not work", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);

        expect(writer.reusable(root / "page.html")).to_be_false();
      });
    });

    spec::context("what it produced", [] {
      spec::it("remembers the paths produced since a mark", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.write(root / "first.html", "one");

        const std::size_t mark = writer.mark();

        writer.write(root / "second.html", "two");

        expect(writer.since(mark)).to_eq(std::vector<std::string>{"second.html"});
      });

      spec::it("remembers nothing produced since a mark at the end", [] {
        const std::filesystem::path root = scratch();

        Writer writer(root);
        writer.write(root / "first.html", "one");

        expect(writer.since(writer.mark())).to_eq(std::vector<std::string>{});
      });

      // A file the build changed some other way is still output, and pruning
      // has to know that.
      spec::it("records a file it did not write itself", [] {
        const std::filesystem::path root = scratch();

        write_file(root / "outside.html", "written elsewhere");

        Writer writer(root);
        writer.record(root / "outside.html");
        writer.prune();

        expect(std::filesystem::exists(root / "outside.html")).to_be_true();
      });
    });

    spec::context("pruning", [] {
      spec::it("removes what this build did not produce", [] {
        const std::filesystem::path root = scratch();

        write_file(root / "stale.html", "old");

        Writer writer(root);
        writer.write(root / "page.html", "body");
        writer.prune();

        expect(std::filesystem::exists(root / "stale.html")).to_be_false();
      });

      spec::it("leaves a keep file alone", [] {
        const std::filesystem::path root = scratch();

        write_file(root / "uploads" / ".keep", "");

        Writer writer(root);
        writer.write(root / "page.html", "body");
        writer.prune();

        expect(std::filesystem::exists(root / "uploads" / ".keep")).to_be_true();
      });

      spec::it("removes a directory left empty", [] {
        const std::filesystem::path root = scratch();

        write_file(root / "old" / "page.html", "old");

        Writer writer(root);
        writer.write(root / "page.html", "body");
        writer.prune();

        expect(std::filesystem::exists(root / "old")).to_be_false();
      });

      spec::it("does nothing when there is no output tree at all", [] {
        const std::filesystem::path root = scratch() / "never-made";

        Writer writer(root);
        writer.prune();

        expect(std::filesystem::exists(root)).to_be_false();
      });
    });

    // Every path is built by joining the root, so this is lexical. One that is
    // not under the root keeps its own name instead of becoming a walk back up
    // out of the tree.
    spec::it("names a path outside the root by its own name", [] {
      const std::filesystem::path root = scratch();

      Writer writer(root);
      writer.record(root.parent_path() / "elsewhere.html");

      expect(writer.since(0).at(0)).to_contain("elsewhere.html");
    });
  });
}
