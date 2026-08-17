#include <chrono>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <thread>

#include "support/spec.h"
#include "watcher.h"

using spec::expect;

namespace {

void write(const std::filesystem::path& path, std::string_view body) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

// A site-shaped tree: content with a section under it, layouts, and an output
// directory that must not be watched.
std::filesystem::path watched_tree() {
  const std::filesystem::path root = spec::scratch_directory("watcher");

  write(root / "content" / "hello.md", "one");
  write(root / "content" / "guide" / "a.md", "two");
  write(root / "layouts" / "base.haml", "%p");
  write(root / "public" / "index.html", "built");

  return root;
}

// Swallows whatever the tree's own creation produced. Both backends report
// changes made shortly before the watch began, so an example asserting quiet has
// to start from quiet.
void settle(blogin::Watcher& watcher) {
  while (watcher.wait(std::chrono::milliseconds(150))) {
  }
}

// Waits for a change, but never for longer than a spec should take. Returns
// whether one arrived.
bool changed_within(blogin::Watcher& watcher, std::chrono::milliseconds limit) {
  const auto deadline = std::chrono::steady_clock::now() + limit;

  while (std::chrono::steady_clock::now() < deadline) {
    if (watcher.wait(std::chrono::milliseconds(100))) {
      return true;
    }
  }

  return false;
}

}  // namespace

SPEC {
  spec::describe("watching a tree", [] {
    spec::context("what it watches", [] {
      spec::it("watches the root and every directory under it", [] {
        const std::filesystem::path root = watched_tree();

        const auto directories = blogin::watchable_directories(root, root / "public");

        expect(directories.size()).to_eq(std::size_t{4});
      });

      // A build writing its output would otherwise wake the watcher that
      // started it, and the preview would rebuild forever.
      spec::it("leaves the output directory out", [] {
        const std::filesystem::path root = watched_tree();

        const auto directories = blogin::watchable_directories(root, root / "public");

        bool found = false;

        for (const std::filesystem::path& directory : directories) {
          if (directory.filename() == "public") {
            found = true;
          }
        }

        expect(found).to_be_false();
      });

      spec::it("refuses to watch nothing at all", [] {
        expect(blogin::Watcher::watch({}, {}).error().message).to_contain("nothing to watch");
      });

      // A root given alongside one of its own parents is redundant, since a
      // recursive watch on the parent already covers it. Given either way
      // round, a change under the child still arrives.
      spec::it("notices a change when a child was named before its parent", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch({root / "layouts", root}, {});

        write(root / "layouts" / "base.haml", "edited");

        expect(changed_within(*watcher.value(), std::chrono::seconds(5))).to_be_true();
      });

      spec::it("notices a change when a parent was named before its child", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch({root, root / "layouts"}, {});

        write(root / "layouts" / "base.haml", "edited");

        expect(changed_within(*watcher.value(), std::chrono::seconds(5))).to_be_true();
      });

      spec::it("counts the directories it is watching", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        expect(watcher.value()->watched()).to_eq(std::size_t{4});
      });
    });

    spec::context("noticing a change", [] {
      spec::it("notices a file being written", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        write(root / "content" / "hello.md", "edited");

        expect(changed_within(*watcher.value(), std::chrono::seconds(5))).to_be_true();
      });

      spec::it("notices a file being added", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        write(root / "content" / "new.md", "fresh");

        expect(changed_within(*watcher.value(), std::chrono::seconds(5))).to_be_true();
      });

      spec::it("notices a file being removed", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        std::filesystem::remove(root / "content" / "hello.md");

        expect(changed_within(*watcher.value(), std::chrono::seconds(5))).to_be_true();
      });

      spec::it("notices a change in a directory below the root", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        write(root / "content" / "guide" / "a.md", "edited");

        expect(changed_within(*watcher.value(), std::chrono::seconds(5))).to_be_true();
      });

      spec::it("notices a layout changing", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        write(root / "layouts" / "base.haml", "%p edited");

        expect(changed_within(*watcher.value(), std::chrono::seconds(5))).to_be_true();
      });

      // A build writes its output constantly. If that woke the watcher, every
      // rebuild would start another one.
      spec::it("ignores a write to the output directory", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        settle(*watcher.value());

        write(root / "public" / "index.html", "rebuilt");

        expect(watcher.value()->wait(std::chrono::milliseconds(300))).to_be_false();
      });

      spec::it("reports nothing when nothing happened", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        settle(*watcher.value());

        expect(watcher.value()->wait(std::chrono::milliseconds(200))).to_be_false();
      });
    });

    // A blocked wait has to be interruptible, or the server cannot be stopped
    // between edits.
    spec::context("stopping", [] {
      spec::it("returns from a wait when told to stop", [] {
        const std::filesystem::path root = watched_tree();

        auto watcher = blogin::Watcher::watch(blogin::watchable_directories(root, root / "public"),
                                              {root / "public"});

        std::thread stopper([&] {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          watcher.value()->stop();
        });

        // Would block for a minute if stopping did nothing, so the example
        // finishing at all is the assertion.
        watcher.value()->wait(std::chrono::seconds(60));
        stopper.join();

        expect(true).to_be_true();
      });
    });
  });

  // FSEvents watches a whole tree from one stream, so the watcher is handed
  // paths the watch list never asked for and has to sort them out itself.
  spec::describe("deciding whether a delivered path is an edit", [] {
    const std::vector<std::string> roots{"/site", "/other"};
    const std::vector<std::string> excluded{"/site/public"};

    const auto ignored = [=](std::string_view path) {
      return blogin::ignored_change(path, roots, excluded);
    };

    spec::it("treats a post as an edit", [=] {
      expect(ignored("/site/content/posts/hello.md")).to_be_false();
    });

    spec::it("treats a layout as an edit", [=] {
      expect(ignored("/site/layouts/base.haml")).to_be_false();
    });

    spec::context("what the build wrote", [=] {
      spec::it("is not an edit", [=] {
        expect(ignored("/site/public/index.html")).to_be_true();
      });

      spec::it("is not an edit even at the output root itself", [=] {
        expect(ignored("/site/public")).to_be_true();
      });

      // A sibling whose name merely starts the same way is a different tree.
      spec::it("does not swallow a directory sharing the prefix", [=] {
        expect(ignored("/site/public-notes/index.md")).to_be_false();
      });
    });

    // A site is usually a git repository, and an editor refreshing .git/index
    // would otherwise wake a rebuild every few seconds with nothing to do.
    spec::context("a dot directory", [=] {
      spec::it("is not an edit", [=] { expect(ignored("/site/.git/index")).to_be_true(); });

      spec::it("is not an edit however deep it sits", [=] {
        expect(ignored("/site/content/.cache/x")).to_be_true();
      });

      spec::it("is not an edit for a nested object file", [=] {
        expect(ignored("/site/.git/objects/ab/cdef")).to_be_true();
      });

      // The rule is about directories. A dotfile someone writes on purpose,
      // such as .keep or .htaccess in static/, is still an edit.
      spec::it("still treats a dotfile as an edit", [=] {
        expect(ignored("/site/static/.htaccess")).to_be_false();
      });
    });

    // The site may itself live under a dot directory, which says nothing about
    // the file that changed.
    spec::it("looks only below the root it was watching", [] {
      expect(blogin::ignored_change("/home/me/.config/site/content/a.md",
                                    {"/home/me/.config/site"}, {}))
        .to_be_false();
    });

    spec::it("ignores a path under no watched root", [=] {
      expect(ignored("/elsewhere/file.md")).to_be_false();
    });
  });
}
