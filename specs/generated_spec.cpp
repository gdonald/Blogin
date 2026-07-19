#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "arena.h"
#include "html.h"
#include "markdown.h"
#include "support/spec.h"
#include "template.h"

using blogin::Arena;
using blogin::CompiledTemplate;
using blogin::Context;
using blogin::Source;
using spec::expect;

namespace {

// A fixed seed keeps a failure reproducible. Nothing here reads the clock.
constexpr std::uint32_t seed = 0x8badf00d;
constexpr int document_count = 4000;

const std::vector<std::string>& fragments() {
  static const std::vector<std::string> pieces{
    "word", " ", "\n", "\n\n", "*", "**", "***", "****",
    "#", "##", "####### ", "# ", "###### ", "\t", "  ",
    "<", ">", "&", "\"", "'", "a", "]", "[", "\\",
    "*unclosed", "**also unclosed", "* *", "**a*b**",
    "%p", "%div ", "= title", "!= yield", "  ", "%",
  };

  return pieces;
}

std::string generate(std::mt19937& engine) {
  std::uniform_int_distribution<std::size_t> piece(0, fragments().size() - 1);
  std::uniform_int_distribution<int> length(0, 40);

  const int parts = length(engine);
  std::string document;

  for (int index = 0; index < parts; ++index) {
    document += fragments()[piece(engine)];
  }

  return document;
}

std::string parse_and_render(std::string markdown) {
  const Source source(std::move(markdown));

  Arena arena;

  return blogin::render_html(blogin::parse_markdown(arena, source));
}

}  // namespace

// These earn their keep under the sanitizers rather than through their
// assertions. Every node in a parsed tree holds a view into the source buffer,
// and this walks enough odd input to catch a view that outlives its buffer or a
// scan that runs off the end.
SPEC {
  spec::describe("generated input", [] {
    spec::context("markdown", [] {
      spec::it("parses without reading outside the source buffer", [] {
        std::mt19937 engine(seed);

        for (int index = 0; index < document_count; ++index) {
          parse_and_render(generate(engine));
        }

        expect(true).to_be_true();
      });

      spec::it("renders the same bytes when the same document is parsed twice", [] {
        std::mt19937 engine(seed);

        spec::aggregate_failures([&] {
          for (int index = 0; index < 500; ++index) {
            const std::string markdown = generate(engine);

            expect(parse_and_render(markdown)).to_eq(parse_and_render(markdown));
          }
        });
      });

      spec::it("survives a long run of emphasis markers", [] {
        expect(parse_and_render(std::string(2000, '*')).empty()).to_be_false();
      });

      spec::it("handles a document with no trailing newline", [] {
        expect(parse_and_render("trailing")).to_eq("<p>trailing</p>\n");
      });
    });

    spec::context("templates", [] {
      spec::it("compiles without reading outside the source buffer", [] {
        std::mt19937 engine(seed);

        const Context context;

        for (int index = 0; index < document_count; ++index) {
          const CompiledTemplate compiled = CompiledTemplate::compile(generate(engine));

          render_template(compiled, context);
        }

        expect(true).to_be_true();
      });
    });
  });
}
