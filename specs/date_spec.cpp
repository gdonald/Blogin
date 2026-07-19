#include <string>

#include "date.h"
#include "support/spec.h"

using blogin::Date;
using spec::expect;

SPEC {
  spec::describe("Date", [] {
    spec::context("parsing", [] {
      spec::it("reads an iso date", [] {
        expect(Date::parse("2024-03-17").value().iso()).to_eq("2024-03-17");
      });

      spec::it("rejects a date with no separators", [] {
        expect(Date::parse("20240317").has_value()).to_be_false();
      });

      spec::it("rejects a short string", [] { expect(Date::parse("2024-3-1").has_value()).to_be_false(); });

      spec::it("rejects trailing text", [] {
        expect(Date::parse("2024-03-17T00:00:00").has_value()).to_be_false();
      });

      spec::it("rejects letters where digits belong", [] {
        expect(Date::parse("20x4-03-17").has_value()).to_be_false();
      });

      // Rolling 2023-02-30 forward to March would silently move a post. Better
      // to refuse it and let the build say so.
      spec::it("rejects a day the month does not have", [] {
        expect(Date::parse("2023-02-30").has_value()).to_be_false();
      });

      spec::it("rejects month zero", [] { expect(Date::parse("2024-00-10").has_value()).to_be_false(); });

      spec::it("rejects month thirteen", [] { expect(Date::parse("2024-13-01").has_value()).to_be_false(); });

      spec::it("accepts the twenty-ninth of February in a leap year", [] {
        expect(Date::parse("2024-02-29").has_value()).to_be_true();
      });

      spec::it("rejects the twenty-ninth of February in a common year", [] {
        expect(Date::parse("2023-02-29").has_value()).to_be_false();
      });

      spec::it("accepts the twenty-ninth of February in a leap century", [] {
        expect(Date::parse("2000-02-29").has_value()).to_be_true();
      });

      spec::it("rejects the twenty-ninth of February in a common century", [] {
        expect(Date::parse("1900-02-29").has_value()).to_be_false();
      });
    });

    spec::context("parsing a filename prefix", [] {
      spec::it("reads the date a filename starts with", [] {
        expect(Date::parse_prefix("2024-03-17-my-post.md").value().iso()).to_eq("2024-03-17");
      });

      spec::it("ignores what follows the date", [] {
        expect(Date::parse_prefix("2024-03-17").value().iso()).to_eq("2024-03-17");
      });

      spec::it("rejects a filename that does not start with a date", [] {
        expect(Date::parse_prefix("my-post.md").has_value()).to_be_false();
      });
    });

    spec::context("fields", [] {
      auto date = spec::let([] { return Date::parse("2024-03-17").value(); });

      spec::it("reports its year", [=] { expect(date().year()).to_eq(2024); });

      spec::it("reports its month", [=] { expect(date().month()).to_eq(3U); });

      spec::it("reports its day", [=] { expect(date().day()).to_eq(17U); });

      spec::it("reports its weekday with Monday as one", [=] { expect(date().weekday()).to_eq(7U); });

      spec::it("reports Monday as one", [] {
        expect(Date::parse("2024-03-18").value().weekday()).to_eq(1U);
      });
    });

    spec::context("formatting", [] {
      auto date = spec::let([] { return Date::parse("2024-03-07").value(); });

      spec::it("writes a four-digit year", [=] { expect(date().format("%Y")).to_eq("2024"); });

      spec::it("writes a padded month", [=] { expect(date().format("%m")).to_eq("03"); });

      spec::it("writes a padded day", [=] { expect(date().format("%d")).to_eq("07"); });

      spec::it("writes an unpadded day", [=] { expect(date().format("%e")).to_eq("7"); });

      spec::it("writes a full month name", [=] { expect(date().format("%B")).to_eq("March"); });

      spec::it("writes an abbreviated month name", [=] { expect(date().format("%b")).to_eq("Mar"); });

      spec::it("writes a full weekday name", [=] { expect(date().format("%A")).to_eq("Thursday"); });

      spec::it("writes an abbreviated weekday name", [=] { expect(date().format("%a")).to_eq("Thu"); });

      spec::it("combines specifiers with literal text", [=] {
        expect(date().format("%B %e, %Y")).to_eq("March 7, 2024");
      });

      spec::it("writes a literal percent", [=] { expect(date().format("100%%")).to_eq("100%"); });

      // A typo that vanished would be harder to notice than one that shows up
      // in the page.
      spec::it("passes an unknown specifier through", [=] { expect(date().format("%Q")).to_eq("%Q"); });

      spec::it("passes a trailing percent through", [=] { expect(date().format("a%")).to_eq("a%"); });

      spec::it("writes nothing for an invalid date", [] { expect(Date().format("%Y")).to_eq(""); });
    });

    spec::context("ordering", [] {
      spec::it("compares by year", [] {
        expect(Date::parse("2023-01-01").value() < Date::parse("2024-01-01").value()).to_be_true();
      });

      spec::it("compares by month within a year", [] {
        expect(Date::parse("2024-01-01").value() < Date::parse("2024-02-01").value()).to_be_true();
      });

      spec::it("compares by day within a month", [] {
        expect(Date::parse("2024-01-01").value() < Date::parse("2024-01-02").value()).to_be_true();
      });

      spec::it("treats identical dates as equal", [] {
        expect(Date::parse("2024-01-01").value() == Date::parse("2024-01-01").value()).to_be_true();
      });
    });

    spec::context("a default date", [] {
      spec::it("is not valid", [] { expect(Date().valid()).to_be_false(); });

      spec::it("writes an empty string", [] { expect(Date().iso()).to_eq(""); });
    });
  });
}
