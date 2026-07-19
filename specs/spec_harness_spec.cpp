#include <memory>
#include <string>
#include <vector>

#include "support/golden.h"
#include "support/spec.h"

using spec::expect;

SPEC {
  spec::describe("the spec harness", [] {
    spec::context("before hooks", [] {
      auto log = spec::let([] { return std::make_shared<std::vector<std::string>>(); });

      spec::before_each([=] { log()->emplace_back("outer"); });

      spec::it("runs the hook before the example", [=] {
        expect(log()->size()).to_eq(std::size_t{1});
      });

      spec::context("nested inside another context", [=] {
        spec::before_each([=] { log()->emplace_back("inner"); });

        spec::it("runs the outer hook before the inner one", [=] {
          spec::aggregate_failures([=] {
            expect(log()->size()).to_eq(std::size_t{2});
            expect((*log())[0]).to_eq("outer");
            expect((*log())[1]).to_eq("inner");
          });
        });
      });

      spec::it("starts each example with fresh state", [=] {
        expect(log()->size()).to_eq(std::size_t{1});
      });
    });

    spec::context("let", [] {
      auto calls = spec::let([] { return std::make_shared<int>(0); });

      spec::it("builds its value once per example no matter how often it is read", [=] {
        auto counted = spec::let([=] {
          ++*calls();
          return std::string("built");
        });

        expect(counted()).to_eq("built");
        expect(counted()).to_eq("built");
        expect(counted()).to_eq("built");
        expect(*calls()).to_eq(1);
      });

      spec::it("does not build its value when nothing reads it", [=] {
        expect(*calls()).to_eq(0);
      });
    });

    spec::context("matchers", [] {
      spec::it("compares strings", [] { expect(std::string("hello")).to_eq("hello"); });

      spec::it("finds a substring", [] { expect(std::string("hello world")).to_contain("lo wo"); });

      spec::it("checks a prefix", [] { expect(std::string("hello")).to_start_with("hel"); });

      spec::it("checks truth", [] { expect(true).to_be_true(); });

      spec::it("checks falsity", [] { expect(false).to_be_false(); });

      spec::it("compares numbers", [] { expect(7).to_be_greater_than(3); });

      spec::it("negates equality", [] { expect(std::string("a")).not_to_eq("b"); });

      spec::it("negates substring search", [] { expect(std::string("abc")).not_to_contain("z"); });
    });

    spec::context("diffing", [] {
      spec::it("reports only the lines that differ", [] {
        const std::string diff = spec::unified_diff("one\ntwo\nthree\n", "one\nTWO\nthree\n");

        spec::aggregate_failures([&] {
          expect(diff).to_contain("-2: two");
          expect(diff).to_contain("+2: TWO");
          expect(diff).not_to_contain("one");
          expect(diff).not_to_contain("three");
        });
      });

      spec::it("reports a line the actual output is missing", [] {
        const std::string diff = spec::unified_diff("one\ntwo\n", "one\n");

        expect(diff).to_contain("-2: two");
      });
    });

    spec::xit("reports a pending example without running it", [] {
      expect(true).to_be_false();
    });

    // Reported as pending rather than passing, so an example whose subject the
    // machine cannot provide never claims to have covered anything.
    spec::it("abandons an example that declares itself pending", [] {
      spec::pending("demonstrating the mechanism");

      expect(true).to_be_false();
    });
  });
}
