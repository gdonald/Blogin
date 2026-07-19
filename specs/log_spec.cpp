#include <memory>
#include <sstream>
#include <string>

#include "log.h"
#include "support/spec.h"

using blogin::Log;
using blogin::LogLevel;
using spec::expect;

namespace {

struct Capture {
  std::ostringstream out;
  std::ostringstream err;
};

}  // namespace

SPEC {
  spec::describe("Log", [] {
    auto capture = spec::let([] { return std::make_shared<Capture>(); });

    spec::context("at the normal level", [=] {
      auto log = spec::let([=] { return Log(LogLevel::normal, capture()->out, capture()->err); });

      spec::it("writes an informational message", [=] {
        log().info("building");

        expect(capture()->out.str()).to_eq("building\n");
      });

      spec::it("writes a step without a newline", [=] {
        log().step("building... ");

        expect(capture()->out.str()).to_eq("building... ");
      });

      spec::it("holds back a verbose message", [=] {
        log().verbose("details");

        expect(capture()->out.str()).to_eq("");
      });

      spec::it("writes a warning to the error stream", [=] {
        log().warn("careful");

        expect(capture()->err.str()).to_eq("careful\n");
      });

      spec::it("keeps warnings off the output stream", [=] {
        log().warn("careful");

        expect(capture()->out.str()).to_eq("");
      });
    });

    spec::context("at the quiet level", [=] {
      auto log = spec::let([=] { return Log(LogLevel::quiet, capture()->out, capture()->err); });

      spec::it("holds back an informational message", [=] {
        log().info("building");

        expect(capture()->out.str()).to_eq("");
      });

      spec::it("holds back a warning", [=] {
        log().warn("careful");

        expect(capture()->err.str()).to_eq("");
      });

      // An error the user cannot see is worse than noise, so this one level
      // never suppresses.
      spec::it("still writes an error", [=] {
        log().error("failed");

        expect(capture()->err.str()).to_eq("failed\n");
      });
    });

    spec::context("at the verbose level", [=] {
      auto log = spec::let([=] { return Log(LogLevel::verbose, capture()->out, capture()->err); });

      spec::it("writes a verbose message", [=] {
        log().verbose("details");

        expect(capture()->out.str()).to_eq("details\n");
      });

      spec::it("still writes an informational message", [=] {
        log().info("building");

        expect(capture()->out.str()).to_eq("building\n");
      });
    });

    spec::context("naming a level", [] {
      spec::it("reads quiet", [] {
        expect(Log::level_from_name("quiet") == LogLevel::quiet).to_be_true();
      });

      spec::it("reads verbose", [] {
        expect(Log::level_from_name("verbose") == LogLevel::verbose).to_be_true();
      });

      spec::it("reads normal", [] {
        expect(Log::level_from_name("normal") == LogLevel::normal).to_be_true();
      });

      spec::it("falls back to normal for an unknown name", [] {
        expect(Log::level_from_name("shouty") == LogLevel::normal).to_be_true();
      });
    });

    spec::it("defaults to the normal level", [] { expect(Log().level() == LogLevel::normal).to_be_true(); });
  });
}
