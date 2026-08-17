#include <string>

#include "post.h"
#include "support/spec.h"

using blogin::Post;
using spec::expect;

namespace {

Post parsed(std::string_view source, std::string_view filename = "a-post.md") {
  return Post::parse(source, filename).value_or(Post{});
}

std::string error_of(std::string_view source, std::string_view filename = "a-post.md") {
  const auto post = Post::parse(source, filename);

  return post ? std::string("no error") : post.error().message;
}

}  // namespace

SPEC {
  spec::describe("Post", [] {
    spec::context("what it refuses", [] {
      // The title falls back to the filename, so only a file with no usable
      // name left has none at all.
      spec::it("refuses a post with no title and no name to fall back on", [] {
        expect(error_of("---\ntitle: \"\"\n---\n", "")).to_contain("missing title");
      });

      spec::it("says which file cannot be read", [] {
        expect(Post::load("no-such-post.md").error().message).to_contain("cannot read");
      });
    });

    spec::context("a filename with directories in it", [] {
      spec::it("takes the title from the last component", [] {
        expect(parsed("---\n---\nbody\n", "posts/deep/hello-world.md").title).to_eq("Hello World");
      });
    });

    spec::context("front matter with nothing after it", [] {
      spec::it("reads an empty body", [] {
        expect(parsed("---\ntitle: Hello\n---").body).to_eq("");
      });
    });

    spec::context("front matter", [] {
      auto post = spec::let([] {
        return parsed("---\ntitle: Hello\ndate: 2024-03-07\ntags: [one, two]\n---\nBody text.\n");
      });

      spec::it("reads the title", [=] { expect(post().title).to_eq("Hello"); });

      spec::it("reads the date", [=] { expect(post().date_string()).to_eq("2024-03-07"); });

      spec::it("reads the tags", [=] { expect(post().tags.size()).to_eq(std::size_t{2}); });

      spec::it("keeps the body", [=] { expect(post().body).to_eq("Body text.\n"); });

      spec::it("derives a slug from the title", [=] { expect(post().slug).to_eq("hello"); });
    });

    spec::context("without front matter", [] {
      spec::it("takes the title from the filename", [] {
        expect(parsed("Just a body.\n", "my-first-post.md").title).to_eq("My First Post");
      });

      spec::it("keeps the whole file as the body", [] {
        expect(parsed("Just a body.\n").body).to_eq("Just a body.\n");
      });
    });

    spec::context("quoting", [] {
      spec::it("strips double quotes", [] {
        expect(parsed("---\ntitle: \"Hello: World\"\n---\n").title).to_eq("Hello: World");
      });

      spec::it("strips single quotes", [] {
        expect(parsed("---\ntitle: 'Hello'\n---\n").title).to_eq("Hello");
      });
    });

    spec::context("dates", [] {
      spec::it("takes one from the filename when the front matter has none", [] {
        expect(parsed("---\ntitle: X\n---\n", "2024-03-07-x.md").date_string()).to_eq("2024-03-07");
      });

      spec::it("prefers the front matter over the filename", [] {
        expect(parsed("---\ntitle: X\ndate: 2020-01-01\n---\n", "2024-03-07-x.md").date_string())
          .to_eq("2020-01-01");
      });

      spec::it("leaves the date empty when there is none anywhere", [] {
        expect(parsed("---\ntitle: X\n---\n", "x.md").date_string()).to_eq("");
      });

      // A date that does not exist is a mistake, so the build stops on it.
      spec::it("refuses a date that is not a date", [] {
        expect(error_of("---\ntitle: X\ndate: soon\n---\n")).to_contain("unparseable date");
      });

      spec::it("names the file it refused", [] {
        expect(error_of("---\ntitle: X\ndate: soon\n---\n", "bad.md")).to_contain("bad.md");
      });
    });

    spec::context("slugs", [] {
      spec::it("prefers an explicit slug", [] {
        expect(parsed("---\ntitle: Hello\nslug: custom\n---\n").slug).to_eq("custom");
      });

      spec::it("strips a date from a filename-derived title", [] {
        expect(parsed("Body\n", "2024-03-07-my-post.md").title).to_eq("My Post");
      });
    });

    spec::context("lists", [] {
      spec::it("reads a bracketed list", [] {
        expect(parsed("---\ntitle: X\ntags: [a, b, c]\n---\n").tags.size()).to_eq(std::size_t{3});
      });

      spec::it("reads a bare list", [] {
        expect(parsed("---\ntitle: X\ntags: a, b\n---\n").tags.size()).to_eq(std::size_t{2});
      });

      spec::it("ignores empty entries", [] {
        expect(parsed("---\ntitle: X\ntags: [a, , b]\n---\n").tags.size()).to_eq(std::size_t{2});
      });

      spec::it("reads aliases", [] {
        expect(parsed("---\ntitle: X\naliases: [/old, /older]\n---\n").aliases.size()).to_eq(std::size_t{2});
      });
    });

    spec::context("flags", [] {
      spec::it("reads draft", [] { expect(parsed("---\ntitle: X\ndraft: true\n---\n").draft).to_be_true(); });

      spec::it("treats anything but true as false", [] {
        expect(parsed("---\ntitle: X\ndraft: yes\n---\n").draft).to_be_false();
      });

      spec::it("reads toc", [] { expect(parsed("---\ntitle: X\ntoc: true\n---\n").toc).to_be_true(); });
    });

    spec::context("order", [] {
      spec::it("reads a whole number", [] {
        expect(parsed("---\ntitle: X\norder: 3\n---\n").order.value()).to_eq(3.0);
      });

      spec::it("reads a fractional order", [] {
        expect(parsed("---\ntitle: X\norder: 2.5\n---\n").order.value()).to_eq(2.5);
      });

      spec::it("leaves it unset when absent", [] {
        expect(parsed("---\ntitle: X\n---\n").order.has_value()).to_be_false();
      });

      spec::it("leaves it unset when it is not a number", [] {
        expect(parsed("---\ntitle: X\norder: first\n---\n").order.has_value()).to_be_false();
      });
    });

    spec::context("keys it does not know", [] {
      auto post = spec::let([] { return parsed("---\ntitle: X\nseries: Raku\n---\n"); });

      spec::it("keeps them", [=] { expect(post().meta.size()).to_eq(std::size_t{1}); });

      spec::it("reads one back by name", [=] {
        expect(std::string(post().meta_value("series"))).to_eq("Raku");
      });

      spec::it("reads nothing for a key that is not there", [=] {
        expect(std::string(post().meta_value("absent"))).to_eq("");
      });
    });

    spec::context("taxonomy terms", [] {
      spec::it("reads tags", [] {
        expect(parsed("---\ntitle: X\ntags: [a, b]\n---\n").terms("tags").size()).to_eq(std::size_t{2});
      });

      spec::it("reads another taxonomy from the meta keys", [] {
        expect(parsed("---\ntitle: X\ntopics: [a, b, c]\n---\n").terms("topics").size()).to_eq(std::size_t{3});
      });

      spec::it("reads nothing for a taxonomy the post does not use", [] {
        expect(parsed("---\ntitle: X\n---\n").terms("topics").size()).to_eq(std::size_t{0});
      });
    });
  });
}
