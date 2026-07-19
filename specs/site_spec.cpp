#include <atomic>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include <map>
#include <random>

#include "counters.h"
#include "files.h"
#include "site.h"
#include "support/spec.h"

using spec::expect;

namespace {

// Its own copy per example, so examples running side by side do not build over
// each other.
std::filesystem::path copy_site(std::string_view name) {
  const std::filesystem::path source = std::filesystem::path(BLOGIN_SPECS_ROOT) / "corpus" / name;
  const std::filesystem::path target = spec::scratch_directory("site");

  std::filesystem::copy(source, target, std::filesystem::copy_options::recursive);

  return target;
}

// The names of the output files that differ between two builds, so a failure
// says which page went wrong rather than printing two whole sites.
std::vector<std::string> differences_between(const std::filesystem::path& left,
                                             const std::filesystem::path& right);

blogin::BuildOptions options_for(const std::filesystem::path& root) {
  auto config = blogin::Config::load(root / "blogin.json").value();

  return blogin::BuildOptions::around(root / "content", config);
}

std::string read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);

  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

// The one file under `directory` whose name starts with `prefix`. A
// fingerprinted asset has no fixed name to ask for.
std::string named_like(const std::filesystem::path& directory, std::string_view prefix) {
  std::error_code error;

  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (entry.path().filename().string().starts_with(prefix)) {
      return entry.path().filename().string();
    }
  }

  return {};
}

std::map<std::string, std::string> output_tree(const std::filesystem::path& root) {
  std::map<std::string, std::string> tree;

  for (const std::filesystem::path& path : blogin::files::all_files(root)) {
    const std::string name = std::filesystem::relative(path, root).generic_string();

    // The manifest and the state are the build's own bookkeeping, and the
    // state records when the build ran, so it differs by design.
    if (name.starts_with(".blogin")) {
      continue;
    }

    tree.emplace(name, read(path));
  }

  return tree;
}

std::vector<std::string> differences_between(const std::filesystem::path& left,
                                             const std::filesystem::path& right) {
  const std::map<std::string, std::string> before = output_tree(left);
  const std::map<std::string, std::string> after = output_tree(right);

  std::vector<std::string> names;

  for (const auto& entry : before) {
    const auto found = after.find(entry.first);

    if (found == after.end() || found->second != entry.second) {
      names.push_back(entry.first);
    }
  }

  for (const auto& entry : after) {
    if (!before.contains(entry.first)) {
      names.push_back(entry.first);
    }
  }

  return names;
}

void write(const std::filesystem::path& path, std::string_view body) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

}  // namespace

SPEC {
  spec::describe("building a site", [] {
    spec::context("a real site", [] {
      auto root = spec::let([] { return copy_site("blogin.dev"); });

      auto report = spec::let([=] { return blogin::build(options_for(root())).value(); });

      spec::it("renders every published post", [=] { expect(report().pages).to_eq(std::size_t{21}); });

      spec::it("writes listings", [=] { expect(report().listings).to_be_greater_than(std::size_t{0}); });

      spec::it("writes a home page", [=] {
        report();

        expect(std::filesystem::exists(root() / "public" / "index.html")).to_be_true();
      });

      spec::it("writes a feed", [=] {
        report();

        expect(read(root() / "public" / "feed.xml")).to_contain("<feed");
      });

      spec::it("writes a sitemap listing every page", [=] {
        report();

        expect(read(root() / "public" / "sitemap.xml")).to_contain("<loc>");
      });

      spec::it("writes robots", [=] {
        report();

        expect(read(root() / "public" / "robots.txt")).to_contain("Sitemap:");
      });

      spec::it("writes a search index", [=] {
        report();

        expect(read(root() / "public" / "search-index.json")).to_contain("\"title\"");
      });

      // This site fingerprints its assets, so the name carries a hash of the
      // content and there is nothing fixed to look for.
      spec::it("writes the search script", [=] {
        report();

        expect(named_like(root() / "public" / "assets" / "js", "search.")).to_contain(".js");
      });

      spec::it("writes the content stylesheet", [=] {
        report();

        expect(read(root() / "public" / "assets" / "css" /
                    named_like(root() / "public" / "assets" / "css", "blogin.")))
          .to_contain(".hl-keyword");
      });

      spec::it("renders a post through its layout", [=] {
        report();

        expect(read(root() / "public" / "guide" / "configuration" / "index.html")).to_contain("<html");
      });

      spec::it("gives a post its title", [=] {
        report();

        expect(read(root() / "public" / "guide" / "configuration" / "index.html")).to_contain("<title>");
      });
    });

    // A post's neighbours are the posts either side of it in the order its own
    // section lists them, which is why they are not a function of its file.
    spec::context("post navigation", [] {
      auto root = spec::let([] { return copy_site("blogin.dev"); });

      auto built = spec::let([=] {
        const blogin::BuildOptions options = options_for(root());

        blogin::build(options);

        return options.output;
      });

      spec::it("links a post to the one before it in its section", [=] {
        expect(read(built() / "guide" / "getting-started" / "index.html"))
          .to_contain(R"(class="prev btn btn-primary" href="/guide/overview/")");
      });

      spec::it("links a post to the one after it", [=] {
        expect(read(built() / "guide" / "overview" / "index.html"))
          .to_contain(R"(class="next btn btn-primary" href="/guide/getting-started/")");
      });

      spec::it("gives the first post in a section nothing to go back to", [=] {
        expect(read(built() / "guide" / "overview" / "index.html")).not_to_contain(R"(class="prev)");
      });

      spec::it("gives the last post in a section nothing to go on to", [=] {
        expect(read(built() / "cli" / "blogin-clean" / "index.html")).not_to_contain(R"(class="next)");
      });
    });

    // A section publishes a feed of its own, so a reader can follow one part of
    // a site without taking all of it.
    spec::context("section feeds", [] {
      auto root = spec::let([] { return copy_site("blogin.dev"); });

      auto built = spec::let([=] {
        const blogin::BuildOptions options = options_for(root());

        blogin::build(options);

        return options.output;
      });

      spec::it("writes one for each section", [=] {
        expect(read(built() / "guide" / "feed.xml")).to_contain("<feed");
      });

      spec::it("names it after the section", [=] {
        expect(read(built() / "guide" / "feed.xml")).to_contain("<title>Blogin: guide</title>");
      });

      spec::it("carries only that section's posts", [=] {
        expect(read(built() / "cli" / "feed.xml")).not_to_contain("/guide/");
      });

      spec::it("writes one in each format the site asked for", [=] {
        spec::aggregate_failures([&] {
          expect(std::filesystem::exists(built() / "guide" / "rss.xml")).to_be_true();
          expect(std::filesystem::exists(built() / "guide" / "feed.json")).to_be_true();
        });
      });
    });

    // What a static host serves for a url it does not have.
    spec::context("a 404 page", [] {
      spec::it("renders the site's own layout when it has one", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "404.html")).to_contain("<html");
      });

      spec::it("falls back to a plain page when the site has no 404 layout", [] {
        const std::filesystem::path root = copy_site("blogin.dev");

        std::filesystem::remove(root / "layouts" / "404.haml");

        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(read(options.output / "404.html"))
          .to_contain("The page you are looking for was not found.");
      });
    });

    // A url a post used to live at, so a link somebody else made still arrives.
    spec::context("aliases", [] {
      spec::it("writes a redirect to the post's own url", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        write(options.content / "guide" / "moved.md",
              "---\ntitle: Moved\naliases: [/old-place]\n---\nHere now.\n");

        blogin::build(options);

        expect(read(options.output / "old-place" / "index.html"))
          .to_contain("url=/guide/moved/");
      });

      spec::it("never replaces a page the content tree publishes", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        write(options.content / "guide" / "moved.md",
              "---\ntitle: Moved\naliases: [/guide/overview]\n---\nHere now.\n");

        blogin::build(options);

        expect(read(options.output / "guide" / "overview" / "index.html")).to_contain("<html");
      });
    });

    // The corpus carries no static or asset trees, so these supply their own.
    spec::context("static files and assets", [] {
      auto root = spec::let([] {
        const std::filesystem::path site = copy_site("blogin.dev");

        std::filesystem::create_directories(site / "static");
        std::filesystem::create_directories(site / "assets" / "css");

        write(site / "static" / "favicon.ico", "icon bytes");
        write(site / "static" / "CNAME", "example.com");
        write(site / "assets" / "css" / "style.css", "body { color: red }");

        return site;
      });

      spec::it("copies a static file to the site root", [=] {
        blogin::build(options_for(root()));

        expect(read(root() / "public" / "favicon.ico")).to_eq("icon bytes");
      });

      spec::it("keeps a static file's own name", [=] {
        blogin::build(options_for(root()));

        expect(std::filesystem::exists(root() / "public" / "CNAME")).to_be_true();
      });

      spec::it("copies assets under assets", [=] {
        blogin::build(options_for(root()));

        expect(read(root() / "public" / "assets" / "css" /
                    named_like(root() / "public" / "assets" / "css", "style.")))
          .to_contain("color:red");
      });

      spec::it("leaves a copied file alone on a rebuild", [=] {
        const blogin::BuildOptions options = options_for(root());

        blogin::build(options);

        expect(blogin::build(options)->written).to_eq(std::size_t{0});
      });
    });

    // The four property checks. Each is about work done rather than time taken,
    // so they mean the same thing on any machine.
    spec::context("properties", [] {
      spec::it("writes nothing on a rebuild that changed nothing", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        expect(blogin::build(options)->written).to_eq(std::size_t{0});
      });

      spec::it("writes only what a single edit affects", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path post = options.content / "guide" / "installation.md";
        write(post, read(post) + "\n\nAn added paragraph.\n");

        // The post, the listings that show it, the feeds, and the search index.
        // Not the whole site.
        expect(blogin::build(options)->written).to_be_less_than(std::size_t{15});
      });

      // Whole trees, not one page: a difference caused by worker ordering is
      // most likely to land in a listing or a feed, which is what naming a
      // single file would miss.
      for (const std::string_view name : {"blogin.dev", "behave.dev", "keayl.dev", "gregdonald.com"}) {
        spec::it("renders " + std::string(name) + " to the same bytes however many workers run", [name] {
          const std::filesystem::path serial_root = copy_site(name);
          const std::filesystem::path parallel_root = copy_site(name);

          blogin::BuildOptions serial = options_for(serial_root);
          serial.jobs = 1;

          blogin::BuildOptions parallel = options_for(parallel_root);
          parallel.jobs = 8;

          blogin::build(serial);
          blogin::build(parallel);

          expect(differences_between(serial.output, parallel.output))
            .to_eq(std::vector<std::string>{});
        });
      }

      // Pruning walks the output directory and deletes what the build did not
      // produce, which is a dangerous shape of code to get wrong.
      spec::it("deletes nothing outside the output directory", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        const std::filesystem::path bystander = root / "notes.txt";
        write(bystander, "not build output");

        blogin::build(options);

        spec::aggregate_failures([&] {
          expect(std::filesystem::exists(bystander)).to_be_true();
          expect(std::filesystem::exists(options.content)).to_be_true();
          expect(std::filesystem::exists(root / "layouts")).to_be_true();
        });
      });

      // Output is written to a temporary name and renamed, so a reader either
      // sees the previous file or the new one.
      spec::it("leaves no partial file behind", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        std::vector<std::string> partial;

        for (const std::filesystem::path& path : blogin::files::all_files(options.output)) {
          if (path.extension() == ".tmp") {
            partial.push_back(path.filename().string());
          }
        }

        expect(partial).to_eq(std::vector<std::string>{});
      });

      spec::it("removes output a later build no longer produces", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path stale = root / "public" / "stale.html";
        write(stale, "left over");

        blogin::build(options);

        expect(std::filesystem::exists(stale)).to_be_false();
      });

      // A .keep is the site owner's, not build output.
      spec::it("leaves a keep file alone", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path keep = root / "public" / "uploads" / ".keep";
        std::filesystem::create_directories(keep.parent_path());
        write(keep, "");

        blogin::build(options);

        expect(std::filesystem::exists(keep)).to_be_true();
      });
    });

    // Incremental builds have a reputation for being subtly wrong, and the
    // reason is almost always invalidation by guesswork. This is the check that
    // makes the reputation not apply: whatever the edits, the incremental
    // result has to be the result a fresh build would have produced.
    spec::context("incremental correctness", [] {
      spec::it("does not parse or render anything when nothing changed", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const auto again = blogin::build(options).value();

        spec::aggregate_failures([&] {
          expect(again.rendered).to_eq(std::size_t{0});
          expect(again.written).to_eq(std::size_t{0});
        });
      });

      spec::it("renders only the post that was edited", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path post = options.content / "guide" / "2026-07-12-configuration.md";
        write(post, read(post) + "\n\nAn added paragraph.\n");

        expect(blogin::build(options)->rendered).to_eq(std::size_t{1});
      });

      // The pages either side of a deleted post are re-read and re-rendered
      // even though their own files did not change, because what they link to
      // is not written in them.
      spec::it("re-renders the pages either side of a post that was deleted", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        std::filesystem::remove(options.content / "guide" / "2026-07-12-configuration.md");

        expect(blogin::build(options)->rendered).to_eq(std::size_t{2});
      });

      spec::it("leaves no page linking to a post that was deleted", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        std::filesystem::remove(options.content / "guide" / "2026-07-12-configuration.md");
        blogin::build(options);

        expect(read(options.output / "guide" / "internationalization" / "index.html"))
          .not_to_contain("/guide/configuration/");
      });

      // A layout can affect any page, so it is not reasoned about.
      spec::it("rebuilds everything when a layout changes", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        const auto first = blogin::build(options).value();

        const std::filesystem::path layout = root / "layouts" / "_footer.haml";
        write(layout, read(layout) + "\n%p an added line\n");

        expect(blogin::build(options)->rendered).to_eq(first.rendered);
      });

      spec::it("rebuilds everything when the configuration changes", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        const auto first = blogin::build(options).value();

        write(root / "blogin.json", read(root / "blogin.json"));
        std::filesystem::last_write_time(root / "blogin.json", std::filesystem::file_time_type::clock::now());

        const auto config = blogin::Config::load(root / "blogin.json").value();
        blogin::BuildOptions changed = blogin::BuildOptions::around(root / "content", config);

        expect(blogin::build(changed)->rendered).to_be_less_than(first.rendered + 1);
      });

      spec::it("notices a post that was deleted", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);

        const std::filesystem::path post = options.content / "guide" / "2026-07-12-configuration.md";
        const std::filesystem::path output = options.output / "guide" / "configuration" / "index.html";

        // Vacuous if it was never there.
        expect(std::filesystem::exists(output)).to_be_true();

        std::filesystem::remove(post);
        blogin::build(options);

        expect(std::filesystem::exists(output)).to_be_false();
      });

      spec::it("notices a post that was added", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        const auto first = blogin::build(options).value();

        write(options.content / "guide" / "brand-new.md", "---\ntitle: Brand New\n---\nFresh.\n");

        expect(blogin::build(options)->pages).to_eq(first.pages + 1);
      });

      // A file whose timestamp falls inside the build that recorded it cannot be
      // judged by that timestamp, because an edit landing just after the build
      // read it would carry the same stamp the build wrote down. Those sources
      // are compared by their bytes instead. Dating a file into the future is
      // how this spec puts one in that window on purpose.
      spec::it("compares the bytes of a source whose timestamp the build cannot vouch for", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        const std::filesystem::path post = options.content / "guide" / "2026-07-12-configuration.md";
        const auto ahead = std::filesystem::file_time_type::clock::now() + std::chrono::hours(1);

        std::filesystem::last_write_time(post, ahead);
        blogin::build(options);

        // Same length, same timestamp, different words.
        std::string body = read(post);
        const std::size_t at = body.find("---", body.find("---") + 3);

        body.replace(at + 4, 6, "ZZZZZZ");
        write(post, body);
        std::filesystem::last_write_time(post, ahead);

        expect(blogin::build(options)->rendered).to_eq(std::size_t{1});
      });

      // Two tags that differ only in characters a slug used to discard would
      // land on one path, and whichever ran last would win. Now the slug keeps
      // them apart, and anything that still collides stops the build.
      spec::it("keeps tags apart that differ only in punctuation", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        write(options.content / "languages.md", "---\ntitle: Languages\ntags: [c, \"c++\"]\n---\nBoth.\n");

        const auto report = blogin::build(options);

        spec::aggregate_failures([&] {
          expect(report.has_value()).to_be_true();
          expect(std::filesystem::exists(options.output / "tags" / "c" / "index.html")).to_be_true();
          expect(std::filesystem::exists(options.output / "tags" / "c-plus-plus" / "index.html"))
            .to_be_true();
        });
      });

      // Every real site, not only the small fixture. A rebuild that writes
      // anything at all is either work that was not needed or output that is
      // not a function of its input, and the two are hard to tell apart later.
      for (const std::string_view name : {"blogin.dev", "behave.dev", "keayl.dev", "gregdonald.com"}) {
        spec::it("writes nothing when rebuilding " + std::string(name), [name] {
          const std::filesystem::path root = copy_site(name);
          const blogin::BuildOptions options = options_for(root);

          blogin::build(options);

          const auto again = blogin::build(options).value();

          std::vector<std::string> rewritten;

          rewritten.reserve(again.changed.size());
          for (const std::filesystem::path& path : again.changed) {
            rewritten.push_back(std::filesystem::relative(path, options.output).generic_string());
          }

          expect(rewritten).to_eq(std::vector<std::string>{});
        });
      }

      // The property itself: any sequence of edits, then compare against a
      // build that had no history to work from.
      spec::it("matches a fresh build after any sequence of edits", [] {
        const std::filesystem::path incremental = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(incremental);

        blogin::build(options);

        // A fixed seed, so a failure reproduces.
        std::mt19937 engine(0x51ed);

        const std::vector<std::filesystem::path> posts =
          blogin::files::files_with_extension(options.content, ".md");

        std::uniform_int_distribution<std::size_t> pick(0, posts.size() - 1);
        std::uniform_int_distribution<int> choose(0, 3);

        for (int round = 0; round < 12; ++round) {
          const std::filesystem::path& post = posts[pick(engine)];

          switch (choose(engine)) {
            case 0:
              write(post, read(post) + "\n\nRound " + std::to_string(round) + ".\n");
              break;
            case 1: {
              std::string body = read(post);
              const auto title = body.find("title:");

              if (title != std::string::npos) {
                body.insert(body.find('\n', title), " (edited)");
                write(post, body);
              }

              break;
            }
            case 2:
              write(options.content / ("added-" + std::to_string(round) + ".md"),
                    "---\ntitle: Added " + std::to_string(round) + "\ntags: [added]\n---\nNew.\n");
              break;
            default: {
              const std::filesystem::path added =
                options.content / ("added-" + std::to_string(round > 0 ? round - 1 : 0) + ".md");

              std::error_code error;
              std::filesystem::remove(added, error);
              break;
            }
          }

          blogin::build(options);
        }

        // The same content, built by something with no history at all.
        const std::filesystem::path fresh = copy_site("blogin.dev");
        std::filesystem::remove_all(fresh / "content");
        std::filesystem::copy(options.content, fresh / "content",
                              std::filesystem::copy_options::recursive);

        const blogin::BuildOptions clean = options_for(fresh);

        blogin::build(clean);

        expect(differences_between(clean.output, options.output)).to_eq(std::vector<std::string>{});
      });
    });

    // A site can be broken in a dozen places, and each has to say which one.
    // Every example here builds a site that is wrong on purpose.
    spec::context("a site that will not build", [] {
      // The smallest site that builds, for each example to break one part of.
      const auto minimal_site = [] {
        const std::filesystem::path root = spec::scratch_directory("broken");

        std::filesystem::create_directories(root / "content");
        std::filesystem::create_directories(root / "layouts");

        write(root / "blogin.json", R"({"title":"Site","base-url":"https://example.com"})");
        write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nBody.\n");
        write(root / "layouts" / "base.haml", "%html\n  %body\n    != yield\n");
        write(root / "layouts" / "show.haml", "%article= title\n");
        write(root / "layouts" / "index.haml", "%ul\n  - for posts -> $post\n    %li= $post<title>\n");

        return root;
      };

      const auto error_of = [](const std::filesystem::path& root) {
        const auto report = blogin::build(options_for(root));

        return report ? std::string("no error") : report.error().message;
      };

      spec::it("names a layout that does not compile", [=] {
        const std::filesystem::path root = minimal_site();
        write(root / "layouts" / "broken.haml", "%a{href: 'x'\n");

        expect(error_of(root)).to_contain("unterminated attribute list");
      });

      spec::it("names a post it cannot read", [=] {
        const std::filesystem::path root = minimal_site();
        write(root / "content" / "dated.md", "---\ntitle: Dated\ndate: the fourth\n---\nBody.\n");

        expect(error_of(root)).to_contain("dated.md");
      });

      spec::it("names a data file it cannot read", [=] {
        const std::filesystem::path root = minimal_site();
        std::filesystem::create_directories(root / "data");
        write(root / "data" / "menu.yaml", "a: 1\n    b: 2\n");

        expect(error_of(root)).to_contain("unexpected indentation");
      });

      spec::it("says when there is no show layout", [=] {
        const std::filesystem::path root = minimal_site();
        std::filesystem::remove(root / "layouts" / "show.haml");

        expect(error_of(root)).to_contain("no 'show' layout");
      });

      spec::it("says when there is no listing layout", [=] {
        const std::filesystem::path root = minimal_site();
        std::filesystem::remove(root / "layouts" / "index.haml");
        std::filesystem::create_directories(root / "content" / "notes");
        write(root / "content" / "notes" / "one.md", "---\ntitle: One\n---\nBody.\n");

        expect(error_of(root)).to_contain("no listing layout");
      });

      spec::it("reports a name a post's layout asks for and the view does not have", [=] {
        const std::filesystem::path root = minimal_site();
        write(root / "layouts" / "show.haml", "%article= nonesuch\n");

        expect(error_of(root)).to_contain("no such name");
      });

      spec::it("reports a name the base layout asks for", [=] {
        const std::filesystem::path root = minimal_site();
        write(root / "layouts" / "base.haml", "%html= nonesuch\n");

        expect(error_of(root)).to_contain("no such name");
      });

      spec::it("reports a name a listing layout asks for", [=] {
        const std::filesystem::path root = minimal_site();
        std::filesystem::create_directories(root / "content" / "notes");
        write(root / "content" / "notes" / "one.md", "---\ntitle: One\n---\nBody.\n");
        write(root / "layouts" / "index.haml", "%ul= nonesuch\n");

        expect(error_of(root)).to_contain("no such name");
      });

      spec::it("reports a name a term page asks for", [=] {
        const std::filesystem::path root = minimal_site();

        write(root / "blogin.json",
              R"({"title":"Site","base-url":"https://example.com","taxonomies":["tags"]})");
        write(root / "content" / "hello.md", "---\ntitle: Hello\ntags: [one]\n---\nBody.\n");
        write(root / "layouts" / "tag.haml", "%ul= nonesuch\n");

        expect(error_of(root)).to_contain("no such name");
      });

      spec::it("reports a name the 404 layout asks for", [=] {
        const std::filesystem::path root = minimal_site();
        write(root / "layouts" / "404.haml", "%p= nonesuch\n");

        expect(error_of(root)).to_contain("no such name");
      });

      // Whichever finished last would otherwise win, silently.
      // Running the command in the wrong directory used to write an empty site
      // and report success, which reads as a site that lost its posts.
      spec::it("says when there is no content directory at all", [=] {
        const std::filesystem::path root = spec::scratch_directory("empty");

        std::filesystem::create_directories(root);
        write(root / "blogin.json", R"({"title":"Site"})");

        expect(error_of(root)).to_contain("Is this a Blogin site?");
      });

      spec::it("builds a site whose content directory is empty", [=] {
        const std::filesystem::path root = spec::scratch_directory("no-posts");

        std::filesystem::create_directories(root / "content");
        std::filesystem::create_directories(root / "layouts");
        write(root / "blogin.json", R"({"title":"Site"})");
        write(root / "layouts" / "base.haml", "%html\n  %body\n    != yield\n");
        write(root / "layouts" / "index.haml", "%h1= heading\n");

        expect(error_of(root)).to_eq("no error");
      });

      spec::it("names two posts that write the same page", [=] {
        const std::filesystem::path root = minimal_site();
        write(root / "content" / "hello.md", "---\ntitle: Hello\nslug: same\n---\nOne.\n");
        write(root / "content" / "other.md", "---\ntitle: Other\nslug: same\n---\nTwo.\n");

        expect(error_of(root)).to_contain("write the same page");
      });
    });

    spec::context("drafts and future posts", [] {
      spec::it("leaves a draft out", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        blogin::BuildOptions options = options_for(root);

        write(options.content / "guide" / "unfinished.md",
              "---\ntitle: Unfinished\ndraft: true\n---\nNot ready.\n");

        const auto without = blogin::build(options)->pages;

        options.drafts = true;

        expect(blogin::build(options)->pages).to_be_greater_than(without);
      });

      spec::it("holds back a post dated ahead of today", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        blogin::BuildOptions options = options_for(root);

        write(options.content / "guide" / "later.md",
              "---\ntitle: Later\ndate: 2999-01-01\n---\nNot yet.\n");

        const auto without = blogin::build(options)->pages;

        options.future = true;

        expect(blogin::build(options)->pages).to_be_greater_than(without);
      });
    });

    spec::context("refusing to build", [] {
      spec::it("refuses two posts that write the same page", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        write(options.content / "guide" / "one.md", "---\ntitle: Same\nslug: same\n---\nA.\n");
        write(options.content / "guide" / "two.md", "---\ntitle: Same\nslug: same\n---\nB.\n");

        expect(blogin::build(options).error().message).to_contain("write the same page");
      });
    });

    spec::context("cleaning", [] {
      spec::it("removes the output tree", [] {
        const std::filesystem::path root = copy_site("blogin.dev");
        const blogin::BuildOptions options = options_for(root);

        blogin::build(options);
        blogin::clean(options.output, root);

        expect(std::filesystem::exists(options.output)).to_be_false();
      });

      // A mistyped output directory must not be able to take something else
      // with it.
      spec::it("refuses a target outside the site", [] {
        const std::filesystem::path root = copy_site("blogin.dev");

        expect(blogin::clean("/tmp", root).error().message).to_contain("refusing to clean");
      });
    });
  });

  // The posts a post is related to are the ones it shares taxonomy terms with.
  // They are the one thing on its page that nothing in its own file says, so
  // they are also what a rebuild has to notice on its behalf.
  spec::describe("related posts", [] {
    // The work counters are process global, and the examples that read them
    // cannot run alongside anything else building a site.
    spec::serial();

    // Alpha and Beta share two tags, Gamma shares one with each of them, and
    // Delta shares none with anybody.
    //
    // Each post is alone in its own section, so no post is another's previous
    // or next. That leaves shared tags as the only thing tying two pages
    // together, which is what these examples are about.
    const auto related_site = [](std::string_view show, std::string_view configuration) {
      const std::filesystem::path root = spec::scratch_directory("related");

      std::filesystem::create_directories(root / "layouts");

      write(root / "blogin.json", configuration);
      write(root / "layouts" / "base.haml", "%html\n  %body\n    != yield\n");
      write(root / "layouts" / "index.haml", "%ul\n  - for posts -> $post\n    %li= $post<title>\n");
      write(root / "layouts" / "show.haml", show);

      const auto post = [&](std::string_view section, std::string_view body) {
        std::filesystem::create_directories(root / "content" / section);

        write(root / "content" / section / "post.md", body);
      };

      post("one", "---\ntitle: Alpha\ndate: 2024-01-01\ntags: [raku, cpp]\n---\nFirst.\n");
      post("two", "---\ntitle: Beta\ndate: 2024-01-02\ntags: [raku, cpp]\n---\nSecond.\n");
      post("three", "---\ntitle: Gamma\ndate: 2024-01-03\ntags: [raku]\n---\nThird.\n");
      post("four", "---\ntitle: Delta\ndate: 2024-01-04\ntags: [zig]\n---\nFourth.\n");

      return root;
    };

    constexpr std::string_view listing_show =
      "%article= title\n"
      "- if has-related\n"
      "  %ul.related\n"
      "    - for related -> $entry\n"
      "      %li= $entry<title>\n";

    constexpr std::string_view plain_show = "%article= title\n";

    constexpr std::string_view plain_config = R"({"title":"Site","base-url":"https://example.com"})";

    const auto page_of = [](const blogin::BuildOptions& options, std::string_view slug) {
      return read(options.output / slug / "index.html");
    };

    spec::it("lists a post sharing a tag", [=] {
      const blogin::BuildOptions options = options_for(related_site(listing_show, plain_config));

      blogin::build(options);

      expect(page_of(options, "one/alpha")).to_contain("<li>Beta</li>");
    });

    spec::it("lists the post sharing the most tags first", [=] {
      const blogin::BuildOptions options = options_for(related_site(listing_show, plain_config));

      blogin::build(options);

      const std::string page = page_of(options, "one/alpha");

      expect(page.find("Beta") < page.find("Gamma")).to_be_true();
    });

    spec::it("leaves out a post sharing no tag", [=] {
      const blogin::BuildOptions options = options_for(related_site(listing_show, plain_config));

      blogin::build(options);

      expect(page_of(options, "one/alpha")).not_to_contain("Delta");
    });

    spec::it("gives a post sharing nothing with anybody no list at all", [=] {
      const blogin::BuildOptions options = options_for(related_site(listing_show, plain_config));

      blogin::build(options);

      expect(page_of(options, "four/delta")).not_to_contain("class=\"related\"");
    });

    spec::it("lists as many posts as the configuration asks for", [=] {
      const blogin::BuildOptions options = options_for(related_site(
        listing_show, R"({"title":"Site","base-url":"https://example.com","related-count":1})"));

      blogin::build(options);

      expect(page_of(options, "one/alpha")).not_to_contain("Gamma");
    });

    // Scoring reads each of a post's terms once, against an index built once.
    // Comparing every post against every other would cost one visit per pair.
    spec::it("visits each of a post's terms once", [=] {
      const blogin::BuildOptions options = options_for(related_site(listing_show, plain_config));

      blogin::reset_counters();
      blogin::build(options);

      expect(blogin::counter_value(blogin::Counter::related_terms)).to_eq(std::uint64_t{6});
    });

    spec::it("works nothing out for a site whose layouts never ask", [=] {
      const blogin::BuildOptions options = options_for(related_site(plain_show, plain_config));

      blogin::reset_counters();
      blogin::build(options);

      expect(blogin::counter_value(blogin::Counter::related_terms)).to_eq(std::uint64_t{0});
    });

    // Retitling one post changes the related list of every post sharing a tag
    // with it, and none of those files changed.
    spec::it("re-renders the posts related to one that was retitled", [=] {
      const std::filesystem::path root = related_site(listing_show, plain_config);
      const blogin::BuildOptions options = options_for(root);

      blogin::build(options);

      write(root / "content" / "three" / "post.md",
            "---\ntitle: Renamed\ndate: 2024-01-03\ntags: [raku]\n---\nThird.\n");

      expect(blogin::build(options)->rendered).to_eq(std::size_t{3});
    });

    spec::it("leaves a retitled post's old title on no page", [=] {
      const std::filesystem::path root = related_site(listing_show, plain_config);
      const blogin::BuildOptions options = options_for(root);

      blogin::build(options);

      write(root / "content" / "three" / "post.md",
            "---\ntitle: Renamed\ndate: 2024-01-03\ntags: [raku]\n---\nThird.\n");
      blogin::build(options);

      expect(page_of(options, "one/alpha")).not_to_contain("Gamma");
    });
  });
}
