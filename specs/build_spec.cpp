#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include "build.h"
#include "counters.h"
#include "support/golden.h"
#include "build_state.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::filesystem::path scratch_dir() {
  const std::filesystem::path directory = std::filesystem::temp_directory_path() / "blogin-specs";

  std::filesystem::create_directories(directory);

  return directory;
}

void write(const std::filesystem::path& path, std::string_view content) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);

  output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

}  // namespace

SPEC {
  spec::describe("building a page", [] {
    // The work counters are process global and the scratch directory is shared,
    // so these examples cannot run alongside anything else touching either.
    spec::serial();

    auto directory = spec::let([] { return scratch_dir(); });

    // A file the build asks about after it has gone.
    spec::context("stamping a file that is not there", [=] {
      spec::it("reads no size", [=] {
        expect(blogin::file_stamp(directory() / "never-written.md").first).to_eq(std::uintmax_t{0});
      });

      spec::it("reads no modification time", [=] {
        expect(blogin::file_stamp(directory() / "never-written.md").second).to_eq(std::int64_t{0});
      });
    });

    spec::context("from markdown through a template", [=] {
      auto markdown = spec::let([=] { return directory() / "post.md"; });
      auto layout = spec::let([=] { return directory() / "layout.haml"; });
      auto output = spec::let([=] { return directory() / "post.html"; });

      spec::before_each([=] {
        write(markdown(), "# Title\n\nSome **bold** text.\n");
        write(layout(), "%html\n  %head\n    %title= title\n  %body\n    != yield\n");

        std::filesystem::remove(output());

        blogin::reset_counters();

        blogin::build_page(markdown(), layout(), output(), "Skeleton");
      });

      spec::it("writes the output file", [=] {
        expect(std::filesystem::exists(output())).to_be_true();
      });

      spec::it("renders the page it was given", [=] {
        spec::expect_golden("build/page.html", blogin::read_file(output()));
      });

      spec::it("reads the markdown and the template", [=] {
        expect(blogin::counter_value(blogin::Counter::files_read)).to_eq(std::uint64_t{2});
      });

      spec::it("writes one file", [=] {
        expect(blogin::counter_value(blogin::Counter::files_written)).to_eq(std::uint64_t{1});
      });

      spec::it("parses one post", [=] {
        expect(blogin::counter_value(blogin::Counter::posts_parsed)).to_eq(std::uint64_t{1});
      });

      spec::it("compiles one template", [=] {
        expect(blogin::counter_value(blogin::Counter::templates_compiled)).to_eq(std::uint64_t{1});
      });

      spec::it("renders one page", [=] {
        expect(blogin::counter_value(blogin::Counter::pages_rendered)).to_eq(std::uint64_t{1});
      });
    });

    spec::context("writing a file", [=] {
      spec::it("leaves no temporary behind", [=] {
        const std::filesystem::path output = directory() / "atomic.html";

        blogin::write_file(output, "content");

        std::filesystem::path temporary = output;
        temporary += ".tmp";

        expect(std::filesystem::exists(temporary)).to_be_false();
      });

      spec::it("replaces an existing file rather than appending to it", [=] {
        const std::filesystem::path output = directory() / "replaced.html";

        blogin::write_file(output, "a much longer original body");
        blogin::write_file(output, "short");

        expect(blogin::read_file(output)).to_eq("short");
      });

      spec::it("creates missing parent directories", [=] {
        const std::filesystem::path output = directory() / "nested" / "deeper" / "page.html";

        std::filesystem::remove_all(directory() / "nested");

        blogin::write_file(output, "made it");

        expect(blogin::read_file(output)).to_eq("made it");
      });
    });

    spec::context("reading a file that is not there", [=] {
      spec::it("says which path failed", [=] {
        const std::filesystem::path missing = directory() / "absent.md";

        std::filesystem::remove(missing);

        try {
          blogin::read_file(missing);

          expect(false).to_be_true();
        } catch (const std::exception& error) {
          expect(std::string(error.what())).to_contain("absent.md");
        }
      });
    });
  });
}
