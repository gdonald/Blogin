#include <algorithm>
#include <atomic>
#include <filesystem>
#include <ios>
#include <fstream>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "config.h"
#include "files.h"
#include "scaffold.h"
#include "site.h"
#include "support/golden.h"
#include "support/spec.h"
#include "writer.h"

using spec::expect;

namespace {

std::filesystem::path copy_site(std::string_view name) {
  const std::filesystem::path source = std::filesystem::path(BLOGIN_SPECS_ROOT) / "corpus" / name;
  const std::filesystem::path target = spec::scratch_directory("acceptance");

  std::filesystem::copy(source, target, std::filesystem::copy_options::recursive);

  return target;
}

// Every output path with the hash of its bytes, one line each, sorted by path.
//
// Goldening whole sites would mean committing thousands of files, and goldening
// a handful of named pages would leave most of the output unwatched. This
// covers all of it in a file that stays small and diffs to the pages that
// moved.
std::string manifest_of(const std::filesystem::path& output) {
  std::vector<std::pair<std::string, std::string>> entries;

  for (const std::filesystem::path& path : blogin::files::all_files(output)) {
    const std::string name = std::filesystem::relative(path, output).generic_string();

    // The build's own bookkeeping, and the state records when the build ran.
    if (name.starts_with(".blogin")) {
      continue;
    }

    entries.emplace_back(name, blogin::content_hash(blogin::files::read_file(path)));
  }

  std::sort(entries.begin(), entries.end());

  std::string text;

  for (const auto& entry : entries) {
    text += entry.second;
    text += "  ";
    text += entry.first;
    text += '\n';
  }

  return text;
}

}  // namespace

SPEC {
  // What someone gets on their first two commands. Nothing in between, no
  // warning to explain away, and the output recorded so a change to any part of
  // the scaffold shows up here.
  spec::describe("a scaffolded site", [] {
    spec::it("builds to the output recorded in its manifest", [] {
      const std::filesystem::path root = spec::scratch_directory("scaffold-acceptance");

      const auto made = blogin::scaffold::init(root, "none", false, "2026-03-04");

      expect(made.has_value()).to_be_true();

      const auto config = blogin::Config::load(root / "blogin.json").value();
      const blogin::BuildOptions options = blogin::BuildOptions::around(root / "content", config);

      const auto report = blogin::build(options);

      spec::aggregate_failures([&] {
        expect(report.has_value()).to_be_true();
        expect(report->warnings).to_eq(std::vector<std::string>{});
      });

      spec::expect_golden("acceptance/scaffold.manifest", manifest_of(options.output));
    });

    // A scaffolded site stopped building at three posts, because two of them
    // shared a tag, the related list rendered through the entry partial, and
    // that partial reads a name only a listing had. One post never reaches it.
    spec::it("still builds once posts are related to each other", [] {
      const std::filesystem::path root = spec::scratch_directory("scaffold-related");

      blogin::scaffold::init(root, "none", false, "2026-03-04");

      for (int index = 1; index <= 3; ++index) {
        const std::string name = std::format("2026-01-0{}-post-{}.md", index, index);
        const std::string body =
          std::format("---\ntitle: Post {}\ndate: 2026-01-0{}\ntags: [shared]\n---\nBody.\n",
                      index, index);

        std::ofstream out(root / "content" / "posts" / name, std::ios::binary | std::ios::trunc);
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
      }

      const auto config = blogin::Config::load(root / "blogin.json").value();
      const blogin::BuildOptions options = blogin::BuildOptions::around(root / "content", config);

      const auto report = blogin::build(options);

      spec::aggregate_failures([&] {
        expect(report.has_value()).to_be_true();

        if (report.has_value()) {
          expect(report->warnings).to_eq(std::vector<std::string>{});
        }
      });
    });

    // Every profile the renderer ships can be scaffolded, so every profile is
    // scaffolded here and built. A framework that scaffolds but does not build
    // is not support.
    for (const std::string_view framework : {"bootstrap5", "pico", "bulma"}) {
      spec::it("builds a " + std::string(framework) + " site to the output recorded in its manifest",
               [framework] {
                 const std::filesystem::path root = spec::scratch_directory("scaffold-acceptance");

                 const auto made = blogin::scaffold::init(root, framework, false, "2026-03-04");

                 expect(made.has_value()).to_be_true();

                 const auto config = blogin::Config::load(root / "blogin.json").value();
                 const blogin::BuildOptions options =
                   blogin::BuildOptions::around(root / "content", config);

                 const auto report = blogin::build(options);

                 spec::aggregate_failures([&] {
                   expect(report.has_value()).to_be_true();
                   expect(report->warnings).to_eq(std::vector<std::string>{});
                 });

                 spec::expect_golden("acceptance/scaffold-" + std::string(framework) + ".manifest",
                                     manifest_of(options.output));
               });
    }
  });

  // The real sites, built end to end and compared against committed output.
  // Anything that changes a byte of any page shows up here and has to be either
  // explained or regenerated on purpose.
  spec::describe("acceptance", [] {
    for (const std::string_view name : {"blogin.dev", "behave.dev", "keayl.dev", "gregdonald.com"}) {
      spec::context(std::string(name), [name] {
        spec::it("builds to the output recorded in its manifest", [name] {
          const std::filesystem::path root = copy_site(name);
          const auto config = blogin::Config::load(root / "blogin.json").value();
          const blogin::BuildOptions options = blogin::BuildOptions::around(root / "content", config);

          const auto report = blogin::build(options);

          expect(report.has_value()).to_be_true();

          spec::expect_golden("acceptance/" + std::string(name) + ".manifest",
                              manifest_of(options.output));
        });
      });
    }
  });
}
