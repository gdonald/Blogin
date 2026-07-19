#include <string>
#include <thread>
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

constexpr int thread_count = 8;
constexpr int renders_per_thread = 200;

std::vector<std::string> run_on_threads(const std::function<std::string()>& work) {
  std::vector<std::string> results(thread_count);
  std::vector<std::thread> workers;

  workers.reserve(thread_count);
  for (int index = 0; index < thread_count; ++index) {
    workers.emplace_back([&results, &work, index] {
      std::string last;

      for (int repeat = 0; repeat < renders_per_thread; ++repeat) {
        last = work();
      }

      results[static_cast<std::size_t>(index)] = std::move(last);
    });
  }

  for (std::thread& worker : workers) {
    worker.join();
  }

  return results;
}

}  // namespace

SPEC {
  spec::describe("rendering from many threads", [] {
    spec::context("sharing one compiled template", [] {
      // If any state behind a render were mutable and shared, this is where it
      // shows up, either as differing bytes or as a sanitizer report.
      spec::it("produces identical bytes on every thread", [] {
        const CompiledTemplate compiled = CompiledTemplate::compile("%article\n  %h1= title\n  %div!= yield\n");

        Context context;
        context.set("title", "Shared");
        context.set_body("<p>body</p>\n");

        const std::string expected = render_template(compiled, context);

        const std::vector<std::string> results =
          run_on_threads([&] { return render_template(compiled, context); });

        spec::aggregate_failures([&] {
          for (const std::string& result : results) {
            expect(result).to_eq(expected);
          }
        });
      });
    });

    spec::context("parsing with a private arena per thread", [] {
      // Each thread owns its arena and its source buffer, and the views inside
      // a tree point into that thread's buffer. This is what catches a view
      // outliving the string it borrows from.
      spec::it("produces identical bytes on every thread", [] {
        const std::vector<std::string> results = run_on_threads([] {
          const Source source("# Heading\n\nA *paragraph* with **weight**.\n");

          Arena arena;

          return blogin::render_html(blogin::parse_markdown(arena, source));
        });

        const std::string expected =
          "<h1>Heading</h1>\n<p>A <em>paragraph</em> with <strong>weight</strong>.</p>\n";

        spec::aggregate_failures([&] {
          for (const std::string& result : results) {
            expect(result).to_eq(expected);
          }
        });
      });
    });
  });
}
