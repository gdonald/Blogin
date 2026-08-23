#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include "haml.h"
#include "post.h"
#include "value.h"
#include "support/spec.h"
#include "template_store.h"
#include "view.h"

using blogin::Value;
using spec::expect;

namespace {

std::filesystem::path corpus_root() {
  return std::filesystem::path(BLOGIN_SPECS_ROOT) / "corpus";
}

std::vector<std::filesystem::path> layout_files() {
  std::vector<std::filesystem::path> found;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(corpus_root())) {
    if (entry.is_regular_file() && entry.path().extension() == ".haml") {
      found.push_back(entry.path());
    }
  }

  std::sort(found.begin(), found.end());

  return found;
}

std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

// What counts as a construct, the name the ledger knows it by, and the decision
// recorded for it. The detectors are deliberately loose: a false positive costs
// a line in the ledger, while a false negative lets a construct through
// unrecorded. The published version of this table is the HAML compatibility
// page on blogin.dev, and the two say the same thing.
struct Construct {
  std::string key;
  std::string pattern;
  std::string status;

  // A substring every match of `pattern` has to contain, or empty when the
  // pattern has no such substring. Four fifths of the pattern-and-file pairs
  // here find nothing, and each of those still costs a scan of the whole file,
  // so a find() that rules the file out first is most of the work avoided. A
  // hint that is not required would undercount, so the broad patterns carry
  // none.
  std::string required = {};
};

const std::vector<Construct>& constructs() {
  static const std::vector<Construct> known{
    {"element", R"(^[ \t]*%[A-Za-z])", "supported"},
    {"class-shorthand", R"(^[ \t]*\.[A-Za-z])", "supported"},
    {"id-shorthand", R"(^[ \t]*#[A-Za-z])", "supported"},
    {"attribute-hash", R"(\{[A-Za-z_-]+:)", "supported", "{"},
    {"attribute-rocket", R"(=>)", "supported", "=>"},
    {"attribute-html", R"(^[ \t]*%[A-Za-z0-9]+\()", "supported", "("},
    {"attribute-nested", R"(\{[A-Za-z_-]+: *\{)", "supported", "{"},
    {"self-closing", R"(/[ \t]*$)", "supported", "/"},
    {"doctype", R"(^[ \t]*!!!)", "supported", "!!!"},
    {"output-escaped", R"(^[ \t]*= )", "supported", "= "},
    {"output-raw", R"(^[ \t]*!= )", "supported", "!= "},
    {"interpolation", R"(#\{)", "supported", "#{"},
    {"comment-silent", R"(^[ \t]*-#)", "supported", "-#"},
    {"comment-html", R"(^[ \t]*/ )", "changed", "/ "},
    {"filter-plain", R"(^[ \t]*:plain[ \t]*$)", "supported", ":plain"},
    {"filter-escaped", R"(^[ \t]*:escaped[ \t]*$)", "supported", ":escaped"},
    {"filter-javascript", R"(^[ \t]*:javascript[ \t]*$)", "supported", ":javascript"},
    {"filter-css", R"(^[ \t]*:css[ \t]*$)", "supported", ":css"},
    {"if", R"(^[ \t]*- if )", "supported", "- if "},
    {"elsif", R"(^[ \t]*- elsif )", "supported", "- elsif "},
    {"else", R"(^[ \t]*- else)", "supported", "- else"},
    {"unless", R"(^[ \t]*- unless )", "supported", "- unless "},
    {"for", R"(^[ \t]*- for )", "supported", "- for "},
    {"render", R"(render\()", "supported", "render("},
    {"render-partial", R"(:partial<)", "supported", ":partial<"},
    {"render-collection", R"(:collection\()", "supported", ":collection("},
    {"render-as", R"(:as<)", "supported", ":as<"},
    {"render-locals", R"(:locals\()", "supported", ":locals("},
    {"yield", R"(yield)", "supported", "yield"},
    {"cache-fragment", R"(cache-fragment)", "changed", "cache-fragment"},
    {"boolean-literal", R"((True|False))", "supported"},
    {"text-escape", R"(^[ \t]*\\)", "supported", "\\"},
    {"inline-text", R"(^[ \t]*[%.#][A-Za-z][^ \t]* [^=!])", "supported"},
  };

  return known;
}

// Compiling a std::regex costs orders of magnitude more than running one, so
// the patterns are compiled once for the process, not once per file.
const std::vector<std::regex>& patterns() {
  static const std::vector<std::regex> compiled = [] {
    std::vector<std::regex> out;
    out.reserve(constructs().size());

    // Copied into a local first. emplace_back forwards by reference, and libc++
    // declares std::regex::multiline without defining it, so binding a
    // reference to the constant itself fails to link.
    const std::regex::flag_type flags = std::regex::multiline;

    for (const Construct& construct : constructs()) {
      out.emplace_back(construct.pattern, flags);
    }

    return out;
  }();

  return compiled;
}

// How often each construct appears across every corpus layout. The walk reads
// 58 files against 33 patterns, so it is done once and read back by every
// example that wants it.
const std::map<std::string, int>& construct_counts() {
  static const std::map<std::string, int> counts = [] {
    std::map<std::string, int> found;

    for (const std::filesystem::path& path : layout_files()) {
      const std::string body = read(path);

      for (std::size_t index = 0; index < constructs().size(); ++index) {
        const Construct& construct = constructs()[index];

        // Every count starts at zero whether or not the pattern is run, so a
        // construct nothing uses is still named in the map.
        int& count = found[construct.key];

        if (!construct.required.empty() && body.find(construct.required) == std::string::npos) {
          continue;
        }

        count += static_cast<int>(
          std::distance(std::sregex_iterator(body.begin(), body.end(), patterns()[index]),
                        std::sregex_iterator()));
      }
    }

    return found;
  }();

  return counts;
}

const Construct* recorded(const std::string& key) {
  for (const Construct& construct : constructs()) {
    if (construct.key == key) {
      return &construct;
    }
  }

  return nullptr;
}

}  // namespace

// The engine has to handle templates it did not have a hand in writing. These
// are the four real sites, unchanged.
SPEC {
  spec::describe("the real layouts", [] {
    spec::it("finds every one of them", [] {
      expect(layout_files().size()).to_eq(std::size_t{58});
    });

    spec::it("compiles every one of them", [] {
      std::vector<std::string> failures;

      for (const std::filesystem::path& path : layout_files()) {
        auto compiled = blogin::haml::Template::compile(read(path), path.filename().string());

        if (!compiled) {
          failures.push_back(path.filename().string() + ": " + compiled.error().describe());
        }
      }

      expect(failures).to_eq(std::vector<std::string>{});
    });

    spec::it("loads a site's layouts as a set", [] {
      const auto store = blogin::TemplateStore::load({corpus_root() / "gregdonald.com" / "layouts"});

      expect(store.has_value()).to_be_true();
    });

    spec::it("resolves a partial by its underscore name", [] {
      const auto store = blogin::TemplateStore::load({corpus_root() / "gregdonald.com" / "layouts"}).value();

      expect(store.has_partial("sidebar")).to_be_true();
    });

    spec::it("resolves a layout by its plain name", [] {
      const auto store = blogin::TemplateStore::load({corpus_root() / "gregdonald.com" / "layouts"}).value();

      expect(store.has("base")).to_be_true();
    });

    // Loose compatibility rots unless something enforces it. Adding a
    // construct to a real site should either pass or say what decision is
    // needed, instead of producing a render bug nobody looks for.
    spec::context("the compatibility ledger", [] {
      // Constructing a std::regex fills a cache inside the process-wide
      // ctype<char> facet, and libstdc++ fills it without synchronising. Two of
      // these examples building patterns at once is a data race in the standard
      // library that ThreadSanitizer reports six times. Nothing under src/ uses
      // std::regex, so the fix belongs here, not in a suppression file.
      spec::serial();

      auto used = spec::let([] { return construct_counts(); });

      spec::it("names every construct the real layouts use", [=] {
        std::vector<std::string> missing;

        for (const auto& entry : used()) {
          if (entry.second > 0 && recorded(entry.first) == nullptr) {
            missing.push_back(entry.first);
          }
        }

        expect(missing).to_eq(std::vector<std::string>{});
      });

      spec::it("gives every construct it names a status", [] {
        std::vector<std::string> unstated;

        for (const Construct& construct : constructs()) {
          if (construct.status != "supported" && construct.status != "changed" &&
              construct.status != "unsupported") {
            unstated.push_back(construct.key);
          }
        }

        expect(unstated).to_eq(std::vector<std::string>{});
      });

      spec::it("finds the constructs it expects to find", [=] {
        spec::aggregate_failures([=] {
          expect(used().at("element")).to_be_greater_than(1000);
          expect(used().at("if")).to_be_greater_than(200);
          expect(used().at("render-partial")).to_be_greater_than(10);
          expect(used().at("cache-fragment")).to_be_greater_than(5);
        });
      });
    });

    // Compiling proves the syntax is understood. Rendering proves the names
    // the layouts ask for are the names the view offers, which is the part no
    // amount of reading the templates would have settled.
    spec::context("rendering with a real view", [] {
      auto sites = spec::let([] {
        return std::vector<std::string>{"behave.dev", "blogin.dev", "gregdonald.com", "keayl.dev"};
      });

      spec::it("renders every layout of every site", [=] {
        std::vector<std::string> failures;

        for (const std::string& site : sites()) {
          const auto store = blogin::TemplateStore::load({corpus_root() / site / "layouts"});

          if (!store) {
            failures.push_back(site + ": " + store.error().message);
            continue;
          }

          blogin::PostView page;

          Value site_values = Value::object();
          site_values.set("title", Value(site));
          site_values.set("base-url", Value("https://" + site));
          site_values.set("author", Value("Greg Donald"));

          page.chrome.site = site_values;
          page.chrome.section = "posts";
          page.chrome.url = "/posts/hello";
          page.chrome.has_header = true;
          page.chrome.has_sidebar = true;
          page.chrome.has_footer = true;

          Value archives = Value::array();
          Value month = Value::object();
          month.set("term", Value("2024-03"));
          month.set("label", Value("March 2024"));
          archives.push(month);

          Value data = Value::object();
          data.set("archives", archives);
          page.chrome.data = data;

          const auto post = blogin::Post::parse("---\ntitle: Hello\ndate: 2024-03-07\n---\nBody.\n",
                                                "hello.md")
                              .value();
          page.post = &post;
          page.body_html = "<p>Body.</p>";
          page.summary = "A summary.";

          Value tag = Value::object();
          tag.set("name", Value("raku"));
          tag.set("url", Value("/tags/raku"));
          page.tags = Value::array({tag});
          page.related = Value::array();

          blogin::ViewContext context = blogin::view::build(page);

          // Listings and posts share their chrome, so the listing names are
          // added too, without rendering each layout twice.
          blogin::ListingView listing;
          listing.chrome = page.chrome;
          listing.entries = Value::array({tag});
          listing.page_urls = {"/posts/"};

          blogin::ViewContext listing_context = blogin::view::build(listing);

          for (const char* name : {"posts", "entries", "heading", "page-number", "total-pages",
                                   "index-dates", "at-root", "pagination-html", "pagination-links"}) {
            if (const Value* value = listing_context.lookup(name)) {
              context.set(name, *value);
            }
          }

          blogin::haml::RenderOptions options;
          options.body = "<p>page body</p>";

          // Every directory the site keeps templates in. A layout in `docs/`
          // is rendered for a post in `docs/<set>`, the choice that decides
          // which `docnav` it reaches.
          std::vector<std::string> directories;

          for (const std::string& name : store->names()) {
            const auto separator = name.rfind('/');

            if (separator != std::string::npos) {
              directories.push_back(name.substr(0, separator));
            }
          }

          for (const std::string& name : store->names()) {
            const auto separator = name.rfind('/');

            // Where the template lives, and where the partials it names
            // are looked up from: two sections each have a `docnav` of their
            // own.
            const std::string directory =
              separator == std::string::npos ? std::string{} : name.substr(0, separator);
            const std::string leaf =
              separator == std::string::npos ? name : name.substr(separator + 1);

            // A partial is rendered by whatever calls it, which gives
            // it the locals it reads. Rendering one on its own would be asking
            // it a question it was never meant to answer.
            if (leaf.starts_with('_')) {
              continue;
            }

            // The sections this template is reached from: its own directory,
            // or the deeper ones under it when the posts live there.
            std::vector<std::string> sections;

            for (const std::string& candidate : directories) {
              if (!directory.empty() && candidate.starts_with(directory + "/")) {
                sections.push_back(candidate);
              }
            }

            if (sections.empty()) {
              sections.push_back(directory);
            }

            const blogin::haml::Template* compiled = store->find(name);

            if (compiled == nullptr) {
              failures.push_back(std::format("{}/{}: the store compiled no template", site, name));
              continue;
            }

            for (const std::string& section : sections) {
              options.partial = [&store, &section](std::string_view partial) {
                return store->find_partial(partial, section);
              };

              auto rendered = blogin::haml::render(*compiled, context, options);

              if (!rendered) {
                failures.push_back(std::format("{}/{}: {}", site, name, rendered.error().describe()));
              }
            }
          }
        }

        expect(failures).to_eq(std::vector<std::string>{});
      });
    });

    // keayl.dev keeps a `_docnav.haml` in each documentation set's directory,
    // and every one of those sets renders through the same `docs/show.haml`.
    // One flat namespace would give all of them whichever was read first.
    spec::context("templates of the same name in different sections", [] {
      auto store = spec::let([] {
        return std::make_shared<blogin::TemplateStore>(
          blogin::TemplateStore::load({corpus_root() / "keayl.dev" / "layouts"}).value());
      });

      spec::it("gives a section its own partial", [=] {
        expect(store()->find_partial("docnav", "docs/ORM-ActiveRecord"))
          .not_to_eq(store()->find_partial("docnav", "docs/MVC-Keayl"));
      });

      spec::it("finds a layout kept in a parent section", [=] {
        expect(store()->has("show", "docs/ORM-ActiveRecord")).to_be_true();
      });

      spec::it("finds a layout kept at the root", [=] {
        expect(store()->has("base", "docs/ORM-ActiveRecord")).to_be_true();
      });

      spec::it("finds nothing for a partial no directory on the path has", [=] {
        expect(store()->has_partial("docnav", "docs")).to_be_false();
      });
    });

    // Earlier search paths win, so a site overrides a theme.
    spec::it("prefers the first search path that has a template", [] {
      const auto store = blogin::TemplateStore::load({corpus_root() / "blogin.dev" / "layouts",
                                                      corpus_root() / "gregdonald.com" / "layouts"})
                           .value();

      expect(store.has("base")).to_be_true();
    });
  });
}
