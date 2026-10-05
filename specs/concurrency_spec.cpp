#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include "arena.h"
#include "html.h"
#include "markdown.h"
#include "support/spec.h"
#include "template.h"
#include "workers.h"

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

// Each call holds a kibibyte on the stack, so the depth is the stack it needs
// in kibibytes. `volatile` keeps the compiler from folding the array away.
std::size_t use_stack(std::size_t kibibytes) {
  volatile char block[1024] = {};
  block[0] = static_cast<char>(kibibytes);

  return kibibytes == 0 ? static_cast<std::size_t>(block[0]) : use_stack(kibibytes - 1) + 1;
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

SPEC {
  spec::describe("running on worker threads", [] {
    spec::it("runs the task once on each thread", [] {
      std::atomic<unsigned> runs{0};

      blogin::run_on_workers(4, [&runs] { runs.fetch_add(1); });

      expect(runs.load()).to_eq(4U);
    });

    spec::it("runs the task on the calling thread when no thread was started", [] {
      std::atomic<unsigned> runs{0};

      blogin::run_on_workers(0, [&runs] { runs.fetch_add(1); });

      expect(runs.load()).to_eq(1U);
    });

    // macOS gives a new thread 512 KiB, which this would overflow.
    spec::it("gives each thread a stack larger than a new thread gets on macOS", [] {
      std::atomic<std::size_t> deepest{0};

      blogin::run_on_workers(2, [&deepest] { deepest.store(use_stack(2048)); });

      expect(deepest.load()).to_eq(std::size_t{2048});
    });
  });
}
