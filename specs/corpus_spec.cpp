#include <filesystem>
#include <string>
#include <vector>

#include "build.h"
#include "config.h"
#include "json.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::filesystem::path corpus_root() {
  return std::filesystem::path(BLOGIN_SPECS_ROOT) / "corpus";
}

std::vector<std::string> site_names() {
  return {"behave.dev", "blogin.dev", "gregdonald.com", "keayl.dev"};
}

std::size_t count_with_extension(const std::filesystem::path& root, std::string_view extension) {
  std::size_t total = 0;

  if (!std::filesystem::exists(root)) {
    return total;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      ++total;
    }
  }

  return total;
}

}  // namespace

// The corpus is the four real sites, committed. Everything a test reads lives
// inside the repository, because the sites themselves do not exist on a CI
// runner or inside the Debian container.
SPEC {
  spec::describe("the site corpus", [] {
    spec::it("is present", [] { expect(std::filesystem::is_directory(corpus_root())).to_be_true(); });

    spec::context("each site", [] {
      spec::it("has its layouts", [] {
        spec::aggregate_failures([] {
          for (const std::string& site : site_names()) {
            expect(std::filesystem::is_directory(corpus_root() / site / "layouts")).to_be_true();
          }
        });
      });

      spec::it("has its content", [] {
        spec::aggregate_failures([] {
          for (const std::string& site : site_names()) {
            expect(std::filesystem::is_directory(corpus_root() / site / "content")).to_be_true();
          }
        });
      });

      spec::it("has its configuration", [] {
        spec::aggregate_failures([] {
          for (const std::string& site : site_names()) {
            expect(std::filesystem::is_regular_file(corpus_root() / site / "blogin.json")).to_be_true();
          }
        });
      });
    });

    // The four real configuration files are the only ones this project has to
    // read, so parsing them tests the parser and the schema against their
    // actual input.
    spec::context("its configuration files", [] {
      spec::it("parses as json", [] {
        spec::aggregate_failures([] {
          for (const std::string& site : site_names()) {
            const auto text = blogin::read_file(corpus_root() / site / "blogin.json");

            expect(blogin::parse_json(text).has_value()).to_be_true();
          }
        });
      });

      spec::it("is accepted by the schema", [] {
        spec::aggregate_failures([] {
          for (const std::string& site : site_names()) {
            const auto text = blogin::read_file(corpus_root() / site / "blogin.json");
            const auto value = blogin::parse_json(text).value();

            expect(blogin::Config::from_value(value).has_value()).to_be_true();
          }
        });
      });
    });

    spec::context("what it carries", [] {
      spec::it("holds every layout from the real sites", [] {
        expect(count_with_extension(corpus_root(), ".haml")).to_eq(std::size_t{58});
      });

      spec::it("holds every post from the real sites", [] {
        expect(count_with_extension(corpus_root(), ".md")).to_eq(std::size_t{342});
      });

      // Photographs, fonts, and icons run to roughly 180 MB across the four
      // sites and say nothing about template or Markdown behaviour, so the
      // refresh script leaves them out. This is what notices if one slips in.
      spec::it("carries no binary assets", [] {
        std::vector<std::string> unexpected;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(corpus_root())) {
          if (!entry.is_regular_file()) {
            continue;
          }

          const std::string extension = entry.path().extension().string();

          if (extension != ".md" && extension != ".haml" && extension != ".json" && extension != ".yaml" &&
              extension != ".yml" && extension != ".html" && extension != ".txt") {
            unexpected.push_back(entry.path().filename().string());
          }
        }

        expect(unexpected).to_eq(std::vector<std::string>{});
      });
    });
  });
}
