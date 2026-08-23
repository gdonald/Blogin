#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

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

// The same post in two languages, under the filename that makes them the same
// post: the slug differs because the title does, and that is the point.
std::filesystem::path bilingual_site() {
  const std::filesystem::path root = spec::scratch_directory("language");

  write(root / "blogin.json",
        R"({"title":"Bilingual","base-url":"https://example.com","languages":["en","fr"],)"
        R"("language-config":{"fr":{"title":"Bilingue"}},"search":false,"minify":false,)"
        R"("fingerprint":false,"home-section":"posts"})");

  write(root / "layouts" / "base.haml",
        "!!! 5\n%html\n  %body\n    %nav.languages\n"
        "      - for languages -> $language\n"
        "        %a{href: \"#{$language<url>}\"}= $language<code>\n"
        "    %p= site-title\n    != yield\n");

  write(root / "layouts" / "show.haml", "%article\n  %h1= title\n  != body\n");
  write(root / "layouts" / "index.haml", "%section\n  %h1= heading\n");

  write(root / "content" / "en" / "posts" / "2026-03-04-hello.md",
        "---\ntitle: Hello There\n---\nIn English.\n");

  write(root / "content" / "fr" / "posts" / "2026-03-04-hello.md",
        "---\ntitle: Bonjour Vous\n---\nEn français.\n");

  write(root / "content" / "en" / "only-english.md", "---\ntitle: Only English\n---\nNo translation.\n");

  return root;
}

blogin::BuildOptions options_for(const std::filesystem::path& root) {
  const auto config = blogin::Config::load(root / "blogin.json").value();

  return blogin::BuildOptions::around(root / "content", config);
}

}  // namespace

SPEC {
  spec::describe("a site in more than one language", [] {
    spec::context("what it writes", [] {
      spec::it("builds each language into its own subtree", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        spec::aggregate_failures([&] {
          expect(std::filesystem::exists(options.output / "en" / "posts" / "hello-there" / "index.html"))
            .to_be_true();
          expect(std::filesystem::exists(options.output / "fr" / "posts" / "bonjour-vous" / "index.html"))
            .to_be_true();
        });
      });

      spec::it("renders each language's own words", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "fr" / "posts" / "bonjour-vous" / "index.html"))
          .to_contain("En français.");
      });

      // Nothing else can sensibly live at the root of a site with no single
      // language.
      spec::it("redirects the root to the first language", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "index.html")).to_contain("url=/en/");
      });

      spec::it("gives a language its own site title", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "fr" / "posts" / "bonjour-vous" / "index.html"))
          .to_contain("<p>Bilingue</p>");
      });

      spec::it("leaves the other language with the site's own title", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "en" / "posts" / "hello-there" / "index.html"))
          .to_contain("<p>Bilingual</p>");
      });
    });

    spec::context("urls", [] {
      spec::it("puts a language's pages under its own code", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "en" / "posts" / "hello-there" / "index.html"))
          .to_contain("/en/posts/hello-there/");
      });

      spec::it("writes each language's listing under its own code", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(std::filesystem::exists(options.output / "fr" / "index.html")).to_be_true();
      });
    });

    // A switcher that sends you to the front page every time is a switcher
    // nobody uses.
    spec::context("the language switcher", [] {
      spec::it("links a post to its translation rather than to a home page", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "en" / "posts" / "hello-there" / "index.html"))
          .to_contain("/fr/posts/bonjour-vous/");
      });

      spec::it("matches translations by filename rather than by slug", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "fr" / "posts" / "bonjour-vous" / "index.html"))
          .to_contain("/en/posts/hello-there/");
      });

      // Falling back to the home page is right here: there is no page to link
      // to, and a link to one that does not exist is worse than a general one.
      spec::it("falls back to a language's home when there is no translation", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "en" / "only-english" / "index.html")).to_contain("href=\"/fr/\"");
      });

      spec::it("offers every language on a listing too", [] {
        const std::filesystem::path root = bilingual_site();
        const blogin::BuildOptions options = options_for(root);

        blogin::build_site(options);

        expect(read(options.output / "en" / "index.html")).to_contain("/fr/");
      });
    });

    spec::context("translation keys", [] {
      spec::it("strips the date from a filename", [] {
        const std::filesystem::path root = bilingual_site();

        blogin::BuildOptions options = options_for(root);
        options.content = root / "content" / "en";

        expect(blogin::translation_paths(options).contains("posts/hello")).to_be_true();
      });

      spec::it("keeps the section in the key", [] {
        const std::filesystem::path root = bilingual_site();

        blogin::BuildOptions options = options_for(root);
        options.content = root / "content" / "en";

        expect(blogin::translation_paths(options).at("posts/hello")).to_eq("posts/hello-there");
      });

      spec::it("names a post outside a section by its stem alone", [] {
        const std::filesystem::path root = bilingual_site();

        blogin::BuildOptions options = options_for(root);
        options.content = root / "content" / "en";

        expect(blogin::translation_paths(options).contains("only-english")).to_be_true();
      });

      // Ten characters in the shape of a date are not a date unless every one
      // of the eight that should be a digit is one.
      spec::it("keeps a filename shaped like a date whose digits are letters", [] {
        const std::filesystem::path root = bilingual_site();

        write(root / "content" / "en" / "posts" / "abcd-ef-gh-hello.md",
              "---\ntitle: Not Dated\n---\nIn English.\n");

        blogin::BuildOptions options = options_for(root);
        options.content = root / "content" / "en";

        expect(blogin::translation_paths(options).contains("posts/abcd-ef-gh-hello")).to_be_true();
      });
    });

    spec::it("writes nothing on a rebuild that changed nothing", [] {
      const std::filesystem::path root = bilingual_site();
      const blogin::BuildOptions options = options_for(root);

      blogin::build_site(options);

      const auto again = blogin::build_site(options);

      spec::aggregate_failures([&] {
        expect(again.has_value() ? std::string{} : again.error().message).to_eq(std::string{});
        expect(again->written).to_eq(std::size_t{0});
      });
    });

    // A site with one language is the ordinary case and must not pay for this.
    spec::it("builds a site with no languages exactly as before", [] {
      const std::filesystem::path root = spec::scratch_directory("language");

      write(root / "blogin.json", R"({"title":"Plain","base-url":"https://example.com","search":false})");
      write(root / "layouts" / "base.haml", "!!! 5\n%html\n  %body\n    != yield\n");
      write(root / "layouts" / "show.haml", "%article\n  %h1= title\n");
      write(root / "layouts" / "index.haml", "%section\n  %h1= heading\n");
      write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nA post.\n");

      const blogin::BuildOptions options = options_for(root);

      blogin::build_site(options);

      expect(std::filesystem::exists(options.output / "hello" / "index.html")).to_be_true();
    });
  });
}
