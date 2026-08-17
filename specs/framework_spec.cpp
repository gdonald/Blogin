#include <string>
#include <vector>

#include "framework.h"
#include "html.h"
#include "markdown.h"
#include "support/spec.h"
#include "view.h"

using blogin::Framework;
using spec::expect;

namespace {

// Every slot the renderer or the view asks a profile for, and what each of the
// four profiles answers. An empty string means the framework styles that
// element without a class, a decision and not an omission, so it
// is written down and asserted the same as any other.
//
// The table is exhaustive on purpose. Sampling a profile proves the sample.
struct Expected {
  std::string slot;
  std::string none;
  std::string bootstrap5;
  std::string pico;
  std::string bulma;
};

const std::vector<Expected>& expectations() {
  static const std::vector<Expected> table{
    // slot                     none  bootstrap5                 pico         bulma
    {"article", "", "", "", "content"},
    {"table", "", "table", "", "table"},
    {"blockquote", "", "blockquote", "", ""},
    {"image", "", "img-fluid", "", "image"},
    {"heading", "", "", "", ""},
    {"list", "", "", "", ""},
    {"definition-list", "", "", "", ""},
    {"code-block", "", "", "", ""},
    {"pagination-nav", "", "", "", "pagination"},
    {"pagination-list", "", "pagination", "", "pagination-list"},
    {"pagination-item", "", "page-item", "", ""},
    {"pagination-link", "", "page-link", "", "pagination-link"},
    {"pagination-active-item", "", "active", "", ""},
    {"pagination-active-link", "", "", "", "is-current"},
    {"nav", "", "nav", "", "navbar"},
    {"container", "", "container", "container", "container"},
    {"tag", "", "badge text-bg-secondary", "", "tag"},
    {"post-nav-button", "", "btn btn-primary", "", "button"},
  };

  return table;
}

std::string expected_for(const Expected& entry, std::string_view profile) {
  if (profile == "none") {
    return entry.none;
  }

  if (profile == "bootstrap5") {
    return entry.bootstrap5;
  }

  if (profile == "pico") {
    return entry.pico;
  }

  return entry.bulma;
}

std::string body_html(std::string_view markdown, std::string_view profile) {
  blogin::Arena arena;

  blogin::RenderOptions options;
  options.framework = Framework::profile(profile);

  return blogin::render_document(blogin::parse_markdown(arena, markdown), options).html;
}

const std::vector<std::string>& profile_names() {
  static const std::vector<std::string> names{"none", "bootstrap5", "pico", "bulma"};

  return names;
}

}  // namespace

SPEC {
  spec::describe("css framework profiles", [] {
    spec::it("ships the four the documentation names", [] {
      std::vector<std::string> names;

      for (const std::string_view name : Framework::names()) {
        names.emplace_back(name);
      }

      expect(names).to_eq(profile_names());
    });

    // A typo in configuration should stop the build.
    spec::context("an unknown name", [] {
      spec::it("is refused", [] {
        try {
          Framework::profile("tailwind");

          expect(false).to_be_true();
        } catch (const std::exception& error) {
          expect(std::string(error.what())).to_contain("unknown css-framework");
        }
      });

      spec::it("is refused with the names that would have worked", [] {
        try {
          Framework::profile("tailwind");
        } catch (const std::exception& error) {
          spec::aggregate_failures([&] {
            for (const std::string& name : profile_names()) {
              expect(std::string(error.what())).to_contain(name);
            }
          });
        }
      });
    });

    spec::context("every slot of every profile", [] {
      for (const std::string& profile : profile_names()) {
        // Every mismatch is collected and reported at once, so a profile that
        // drifted names each slot it drifted on, not only the first.
        spec::it("answers each slot the way " + profile + " is documented to", [profile] {
          const Framework framework = Framework::profile(profile);

          std::vector<std::string> wrong;

          for (const Expected& entry : expectations()) {
            const std::string actual(framework.class_for(entry.slot));
            const std::string wanted = expected_for(entry, profile);

            if (actual != wanted) {
              std::string line = entry.slot;
              line += ": expected '";
              line += wanted;
              line += "', got '";
              line += actual;
              line += "'";

              wrong.push_back(std::move(line));
            }
          }

          expect(wrong).to_eq(std::vector<std::string>{});
        });
      }

      // A slot nothing defines reads as empty and not as an error, which is
      // what lets the renderer ask for one every profile leaves bare.
      spec::it("answers a slot no profile defines with nothing", [] {
        spec::aggregate_failures([] {
          for (const std::string& profile : profile_names()) {
            expect(std::string(Framework::profile(profile).class_for("no-such-slot"))).to_eq("");
          }
        });
      });
    });

    spec::context("the assets each profile links", [] {
      spec::it("gives bootstrap a stylesheet and a script", [] {
        spec::aggregate_failures([] {
          expect(std::string(Framework::profile("bootstrap5").stylesheet())).to_contain("bootstrap");
          expect(std::string(Framework::profile("bootstrap5").script())).to_contain("bootstrap.bundle");
        });
      });

      spec::it("gives pico a stylesheet", [] {
        expect(std::string(Framework::profile("pico").stylesheet())).to_contain("pico");
      });

      spec::it("gives bulma a stylesheet", [] {
        expect(std::string(Framework::profile("bulma").stylesheet())).to_contain("bulma");
      });

      // Only Bootstrap ships components that need JavaScript, so every other
      // profile renders an empty script tag.
      spec::it("gives no script to the profiles that ship none", [] {
        spec::aggregate_failures([] {
          expect(std::string(Framework::profile("none").script())).to_eq("");
          expect(std::string(Framework::profile("pico").script())).to_eq("");
          expect(std::string(Framework::profile("bulma").script())).to_eq("");
        });
      });

      spec::it("gives none no stylesheet either", [] {
        expect(std::string(Framework::profile("none").stylesheet())).to_eq("");
      });
    });
  });

  // The classes above have to reach the page. These render real Markdown
  // through each profile and read the markup back.
  spec::describe("what a profile does to rendered markup", [] {
    spec::context("a table", [] {
      spec::it("is bare under none", [] {
        expect(body_html("| a |\n|---|\n| 1 |\n", "none")).to_contain("<table>");
      });

      spec::it("is bare under pico", [] {
        expect(body_html("| a |\n|---|\n| 1 |\n", "pico")).to_contain("<table>");
      });

      spec::it("carries bootstrap's class", [] {
        expect(body_html("| a |\n|---|\n| 1 |\n", "bootstrap5"))
          .to_contain("<table class=\"table\">");
      });

      spec::it("carries bulma's class", [] {
        expect(body_html("| a |\n|---|\n| 1 |\n", "bulma")).to_contain("<table class=\"table\">");
      });
    });

    spec::context("a blockquote", [] {
      spec::it("carries bootstrap's class", [] {
        expect(body_html("> quoted\n", "bootstrap5")).to_contain("<blockquote class=\"blockquote\">");
      });

      // Bulma styles a blockquote through the content wrapper, so the element
      // itself stays bare.
      spec::it("is bare under bulma", [] {
        expect(body_html("> quoted\n", "bulma")).to_contain("<blockquote>");
      });

      spec::it("is bare under none", [] {
        expect(body_html("> quoted\n", "none")).to_contain("<blockquote>");
      });
    });

    spec::context("an image", [] {
      spec::it("carries bootstrap's class", [] {
        expect(body_html("![a](/x.png)", "bootstrap5")).to_contain("img-fluid");
      });

      spec::it("carries bulma's class", [] {
        expect(body_html("![a](/x.png)", "bulma")).to_contain("class=\"image\"");
      });

      spec::it("is bare under none", [] {
        expect(body_html("![a](/x.png)", "none")).not_to_contain("class=");
      });
    });

    // Bulma is the reason this wrapper exists. Bare headings, lists,
    // blockquotes, and tables carry no Bulma styling until they sit inside
    // `.content`, so a Bulma site without it renders unstyled prose.
    spec::context("the content wrapper", [] {
      spec::it("wraps the body under bulma", [] {
        expect(body_html("# Title\n\nSome text.\n", "bulma")).to_contain("<div class=\"content\">");
      });

      spec::it("closes what it opened", [] {
        expect(body_html("# Title\n\nSome text.\n", "bulma")).to_contain("</div>");
      });

      spec::it("keeps the body inside it", [] {
        expect(body_html("# Title\n", "bulma")).to_contain("<div class=\"content\">\n<h1>Title</h1>");
      });

      spec::it("is absent under bootstrap", [] {
        expect(body_html("# Title\n", "bootstrap5")).not_to_contain("<div class=\"content\">");
      });

      spec::it("is absent under pico", [] {
        expect(body_html("# Title\n", "pico")).not_to_contain("<div");
      });

      spec::it("is absent under none", [] {
        expect(body_html("# Title\n", "none")).not_to_contain("<div");
      });

      // An empty document produces an empty body, not an empty wrapper,
      // so a post with no content does not gain a stray div.
      spec::it("is absent from an empty document", [] {
        expect(body_html("", "bulma")).to_eq("");
      });
    });

    // Every framework here styles bare headings and lists, either directly or
    // through a wrapper, so the renderer adds nothing to them. Asserted so a
    // profile that starts classing them has to say so here first.
    spec::context("elements every profile leaves bare", [] {
      spec::it("leaves a heading unclassed under every profile", [] {
        spec::aggregate_failures([] {
          for (const std::string& profile : profile_names()) {
            expect(body_html("# Title\n", profile)).to_contain("<h1>Title</h1>");
          }
        });
      });

      spec::it("leaves a list unclassed under every profile", [] {
        spec::aggregate_failures([] {
          for (const std::string& profile : profile_names()) {
            expect(body_html("- one\n- two\n", profile)).to_contain("<ul>");
          }
        });
      });
    });
  });
}
