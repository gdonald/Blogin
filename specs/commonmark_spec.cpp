#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <string>
#include <vector>

#include "arena.h"
#include "html.h"
#include "markdown.h"
#include "support/commonmark.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::string render(const std::string& markdown) {
  blogin::Arena arena;

  return blogin::render_html(blogin::parse_markdown(arena, markdown));
}

}  // namespace

// The specification carries its own test suite. Failures are triaged rather
// than hidden: the report says which sections are weak, and the total is
// asserted against a floor that only ever moves up.
SPEC {
  spec::describe("CommonMark conformance", [] {
    spec::it("finds the vendored specification", [] {
      expect(spec::load_commonmark_examples().size()).to_be_greater_than(std::size_t{600});
    });

    spec::it("meets the conformance floor", [] {
      const auto examples = spec::load_commonmark_examples();

      const char* show_section = std::getenv("BLOGIN_CM_SHOW");
      int shown = 0;

      spec::ConformanceReport report;
      std::vector<std::string> order;

      for (const auto& example : examples) {
        auto found = std::find_if(report.sections.begin(), report.sections.end(),
                                  [&](const auto& section) { return section.name == example.section; });

        if (found == report.sections.end()) {
          report.sections.push_back({example.section, 0, 0});
          found = report.sections.end() - 1;
        }

        const std::string actual = render(example.markdown);
        const bool passed = actual == example.html;

        // Set BLOGIN_CM_SHOW to a section name to see what that section is
        // still getting wrong. Diagnostics, not a gate.
        if (!passed && show_section != nullptr && example.section.contains(show_section) &&
            shown < 6) {
          ++shown;
          std::print("\n--- example {} ({})\ninput:\n{}\nexpected:\n{}\nactual:\n{}",
                      example.number, example.section, example.markdown,
                      example.html, actual);
        }

        ++found->total;
        ++report.total;

        if (passed) {
          ++found->passed;
          ++report.passed;
        }
      }

      std::println("\n{}", report.describe());

      expect(report.passed).to_be_greater_than(BLOGIN_COMMONMARK_FLOOR);
    });
  });
}
