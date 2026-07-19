#include <string>

#include "counters.h"
#include "support/spec.h"

using blogin::Counter;
using spec::expect;

SPEC {
  spec::describe("work counters", [] {
    // The counters are process global, which is why these examples cannot share
    // the process with anything else reading them.
    spec::serial();

    spec::before_each([] { blogin::reset_counters(); });

    spec::it("starts at zero", [] {
      expect(blogin::counter_value(Counter::pages_rendered)).to_eq(std::uint64_t{0});
    });

    spec::it("counts one by default", [] {
      blogin::count(Counter::pages_rendered);

      expect(blogin::counter_value(Counter::pages_rendered)).to_eq(std::uint64_t{1});
    });

    spec::it("adds the amount it is given", [] {
      blogin::count(Counter::files_read, 7);

      expect(blogin::counter_value(Counter::files_read)).to_eq(std::uint64_t{7});
    });

    spec::it("accumulates across calls", [] {
      blogin::count(Counter::files_written);
      blogin::count(Counter::files_written, 4);

      expect(blogin::counter_value(Counter::files_written)).to_eq(std::uint64_t{5});
    });

    spec::it("keeps each counter separate", [] {
      blogin::count(Counter::posts_parsed, 3);

      expect(blogin::counter_value(Counter::templates_compiled)).to_eq(std::uint64_t{0});
    });

    spec::it("resets every counter", [] {
      blogin::count(Counter::posts_parsed, 3);
      blogin::count(Counter::directory_walks, 2);

      blogin::reset_counters();

      spec::aggregate_failures([] {
        expect(blogin::counter_value(Counter::posts_parsed)).to_eq(std::uint64_t{0});
        expect(blogin::counter_value(Counter::directory_walks)).to_eq(std::uint64_t{0});
      });
    });

    spec::it("names each counter", [] {
      expect(std::string(blogin::counter_name(Counter::pages_rendered))).to_eq("pages_rendered");
    });

    spec::context("the report", [] {
      spec::it("lists a counter and its value", [] {
        blogin::count(Counter::pages_rendered, 12);

        expect(blogin::counters_report()).to_contain("pages_rendered: 12");
      });

      spec::it("lists every counter", [] {
        const std::string report = blogin::counters_report();

        spec::aggregate_failures([&] {
          expect(report).to_contain("files_read:");
          expect(report).to_contain("files_written:");
          expect(report).to_contain("posts_parsed:");
          expect(report).to_contain("templates_compiled:");
          expect(report).to_contain("pages_rendered:");
          expect(report).to_contain("directory_walks:");
        });
      });
    });
  });
}
