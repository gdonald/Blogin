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

// Three pages and a sidebar that marks whichever of them is being rendered.
//
// The sidebar is wrapped in cache-fragment under one name, which is an author
// saying "this comes out the same everywhere". It does not, and the key is
// derived from what the sidebar read rather than from that name, so the claim
// costs nothing.
std::filesystem::path site_with_a_current_page_sidebar() {
  const std::filesystem::path root = spec::scratch_directory("fragment");

  write(root / "blogin.json",
        R"({"title":"Fragments","base-url":"https://example.com","search":false,)"
        R"("minify":false,"fingerprint":false})");

  write(root / "content" / "one.md", "---\ntitle: One\n---\nFirst.\n");
  write(root / "content" / "two.md", "---\ntitle: Two\n---\nSecond.\n");
  write(root / "content" / "three.md", "---\ntitle: Three\n---\nThird.\n");

  write(root / "data" / "pages.yaml",
        "- url: /one/\n  title: One\n"
        "- url: /two/\n  title: Two\n"
        "- url: /three/\n  title: Three\n");

  write(root / "layouts" / "base.haml",
        "!!! 5\n%html\n  %body\n"
        "    != cache-fragment('sidebar', { render(:partial<sidebar>) })\n"
        "    != yield\n");

  write(root / "layouts" / "_sidebar.haml",
        "%ul.sidebar\n"
        "  - for data<pages> -> $page\n"
        "    - if $page<url> eq url\n"
        "      %li.here= $page<title>\n"
        "    - else\n"
        "      %li= $page<title>\n");

  write(root / "layouts" / "show.haml", "%article\n  %h1= title\n");

  return root;
}

// A fragment reading only site-level values, so every page can share one
// rendering of it.
std::filesystem::path site_with_a_fixed_sidebar() {
  const std::filesystem::path root = site_with_a_current_page_sidebar();

  write(root / "layouts" / "_sidebar.haml",
        "%ul.sidebar\n"
        "  - for data<pages> -> $page\n"
        "    %li= $page<title>\n");

  return root;
}

// One worker, so a count of renders is exact rather than a race: two threads
// reaching a fragment before either has finished it both render it, which is
// correct and bounded by the worker count rather than by the page count.
blogin::BuildOptions options_for(const std::filesystem::path& root) {
  const auto config = blogin::Config::load(root / "blogin.json").value();

  blogin::BuildOptions options = blogin::BuildOptions::around(root / "content", config);
  options.jobs = 1;

  return options;
}

std::filesystem::path corpus_copy(std::string_view name) {
  const std::filesystem::path source = std::filesystem::path(BLOGIN_SPECS_ROOT) / "corpus" / name;
  const std::filesystem::path target = spec::scratch_directory("fragment-corpus");

  std::filesystem::copy(source, target, std::filesystem::copy_options::recursive);

  return target;
}

}  // namespace

SPEC {
  spec::describe("fragment reuse", [] {
    // The case an author-keyed cache gets wrong, and gets wrong silently: the
    // first page's sidebar is served to every page, with the wrong entry marked
    // and no error anywhere.
    spec::context("a sidebar that marks the current page", [] {
      spec::it("marks each page on its own page", [] {
        const std::filesystem::path root = site_with_a_current_page_sidebar();
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        spec::aggregate_failures([&] {
          expect(read(options.output / "one" / "index.html")).to_contain(R"(<li class="here">One</li>)");
          expect(read(options.output / "two" / "index.html")).to_contain(R"(<li class="here">Two</li>)");
          expect(read(options.output / "three" / "index.html")).to_contain(R"(<li class="here">Three</li>)");
        });
      });

      spec::it("marks nobody else on a page", [] {
        const std::filesystem::path root = site_with_a_current_page_sidebar();
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "two" / "index.html")).not_to_contain(R"(<li class="here">One</li>)");
      });

      // Reading page state is what decides it, not the name the author gave.
      spec::it("reuses nothing across pages it comes out differently on", [] {
        const std::filesystem::path root = site_with_a_current_page_sidebar();
        const blogin::BuildOptions options = options_for(root);

        const auto report = blogin::build(options);

        expect(report->fragments_reused).to_eq(std::size_t{0});
      });

      spec::it("renders it once for each page", [] {
        const std::filesystem::path root = site_with_a_current_page_sidebar();
        const blogin::BuildOptions options = options_for(root);

        const auto report = blogin::build(options);

        expect(report->fragments_rendered).to_eq(std::size_t{3});
      });
    });

    // Nothing was annotated to say this one is reusable. It reads only the data
    // file, so it is, and the build works that out for itself.
    spec::context("a sidebar that reads nothing page-specific", [] {
      spec::it("renders once for the whole build", [] {
        const std::filesystem::path root = site_with_a_fixed_sidebar();
        const blogin::BuildOptions options = options_for(root);

        const auto report = blogin::build(options);

        expect(report->fragments_rendered).to_eq(std::size_t{1});
      });

      spec::it("reuses it on every page after the first", [] {
        const std::filesystem::path root = site_with_a_fixed_sidebar();
        const blogin::BuildOptions options = options_for(root);

        const auto report = blogin::build(options);

        expect(report->fragments_reused).to_eq(std::size_t{2});
      });

      spec::it("puts the same sidebar on every page", [] {
        const std::filesystem::path root = site_with_a_fixed_sidebar();
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        spec::aggregate_failures([&] {
          expect(read(options.output / "one" / "index.html")).to_contain("<li>Three</li>");
          expect(read(options.output / "three" / "index.html")).to_contain("<li>One</li>");
        });
      });
    });

    // The site the manual cache was written for. Its header, sidebar, and
    // footer go on every page it writes, and each is rendered once.
    spec::context("gregdonald.com", [] {
      spec::it("renders its chrome once rather than once a page", [] {
        const std::filesystem::path root = corpus_copy("gregdonald.com");
        const blogin::BuildOptions options = options_for(root);

        const auto report = blogin::build(options);

        expect(report->fragments_rendered).to_eq(std::size_t{3});
      });

      spec::it("reuses it on every other page it writes", [] {
        const std::filesystem::path root = corpus_copy("gregdonald.com");
        const blogin::BuildOptions options = options_for(root);

        const auto report = blogin::build(options);

        // Three fragments on each page, each listing, and the 404 page, all but
        // the first rendering of each coming out of the cache.
        expect(report->fragments_reused)
          .to_eq(((report->pages + report->listings + 1) * 3) - report->fragments_rendered);
      });

      // Threads racing to be the first to render a fragment is the one thing
      // that costs an extra rendering of it, and what it costs is bounded by
      // the number of workers rather than by the number of pages.
      spec::it("renders it a handful of times with sixteen workers rather than one per page", [] {
        const std::filesystem::path root = corpus_copy("gregdonald.com");
        blogin::BuildOptions options = options_for(root);
        options.jobs = 16;

        const auto report = blogin::build(options);

        expect(report->fragments_rendered).to_be_less_than(std::size_t{3} * 16);
      });
    });
  });
}
