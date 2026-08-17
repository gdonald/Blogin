#include <string>
#include <string_view>

#include "support/spec.h"
#include "value.h"

using blogin::Value;
using spec::expect;

SPEC {
  spec::describe("Value", [] {
    spec::context("what it holds", [] {
      spec::it("is null by default", [] { expect(Value().is_null()).to_be_true(); });

      spec::it("holds a boolean", [] { expect(Value(true).as_boolean()).to_be_true(); });

      spec::it("holds an integer", [] { expect(Value(42).as_integer()).to_eq(std::int64_t{42}); });

      spec::it("holds a number", [] { expect(Value(2.5).as_number()).to_eq(2.5); });

      spec::it("holds a string view", [] {
        const std::string_view view = "viewed";

        expect(std::string(Value(view).as_string())).to_eq("viewed");
      });

      spec::it("holds a string", [] { expect(std::string(Value("text").as_string())).to_eq("text"); });

      spec::it("holds an array", [] { expect(Value::array().is_array()).to_be_true(); });

      spec::it("holds an object", [] { expect(Value::object().is_object()).to_be_true(); });

      spec::it("names its type", [] { expect(Value(1).type_name()).to_eq("integer"); });
    });

    spec::context("reading as the wrong type", [] {
      spec::it("falls back rather than throwing", [] {
        expect(Value("text").as_integer(7)).to_eq(std::int64_t{7});
      });

      spec::it("reads an integer as a number", [] { expect(Value(3).as_number()).to_eq(3.0); });

      spec::it("falls back when the value is not a number at all", [] {
        expect(Value("text").as_number(1.5)).to_eq(1.5);
      });

      spec::it("reads a number as an integer by truncating", [] {
        expect(Value(3.9).as_integer()).to_eq(std::int64_t{3});
      });

      spec::it("falls back for a string read from a number", [] {
        expect(std::string(Value(1).as_string("none"))).to_eq("none");
      });
    });

    spec::context("truthiness", [] {
      spec::it("treats null as false", [] { expect(Value().truthy()).to_be_false(); });

      spec::it("treats false as false", [] { expect(Value(false).truthy()).to_be_false(); });

      spec::it("treats zero as false", [] { expect(Value(0).truthy()).to_be_false(); });

      spec::it("treats a zero number as false", [] { expect(Value(0.0).truthy()).to_be_false(); });

      spec::it("treats a non-zero number as true", [] { expect(Value(2.5).truthy()).to_be_true(); });

      spec::it("treats an empty string as false", [] { expect(Value("").truthy()).to_be_false(); });

      spec::it("treats an empty array as false", [] { expect(Value::array().truthy()).to_be_false(); });

      spec::it("treats an empty object as false", [] { expect(Value::object().truthy()).to_be_false(); });

      spec::it("treats a non-empty string as true", [] { expect(Value("x").truthy()).to_be_true(); });

      spec::it("treats a non-zero number as true", [] { expect(Value(1).truthy()).to_be_true(); });

      spec::it("treats a non-empty array as true", [] {
        expect(Value::array({Value(1)}).truthy()).to_be_true();
      });
    });

    spec::context("arrays", [] {
      auto numbers = spec::let([] { return Value::array({Value(1), Value(2), Value(3)}); });

      spec::it("reports its size", [=] { expect(numbers().size()).to_eq(std::size_t{3}); });

      spec::it("reads by index", [=] { expect(numbers().at(1).as_integer()).to_eq(std::int64_t{2}); });

      spec::it("reads past the end as null", [=] { expect(numbers().at(99).is_null()).to_be_true(); });

      spec::it("appends", [] {
        Value list = Value::array();
        list.push(Value("first"));

        expect(list.size()).to_eq(std::size_t{1});
      });

      spec::it("becomes an array when pushed onto", [] {
        Value value;
        value.push(Value(1));

        expect(value.is_array()).to_be_true();
      });
    });

    spec::context("objects", [] {
      auto config = spec::let([] {
        Value value = Value::object();
        value.set("title", Value("Blogin"));
        value.set("page-size", Value(10));

        return value;
      });

      spec::it("reads a key", [=] { expect(std::string(config()["title"].as_string())).to_eq("Blogin"); });

      spec::it("reads a missing key as null", [=] { expect(config()["absent"].is_null()).to_be_true(); });

      spec::it("reports whether a key is present", [=] { expect(config().contains("title")).to_be_true(); });

      spec::it("reports a missing key as absent", [=] { expect(config().contains("absent")).to_be_false(); });

      spec::it("replaces the value of an existing key", [=] {
        Value value = config();
        value.set("title", Value("Changed"));

        spec::aggregate_failures([&] {
          expect(std::string(value["title"].as_string())).to_eq("Changed");
          expect(value.size()).to_eq(std::size_t{2});
        });
      });

      // Configuration is read and written back, and golden files compare bytes,
      // so a container that reordered keys would turn every write into a
      // spurious diff.
      spec::it("keeps keys in the order they were set", [] {
        Value value = Value::object();
        value.set("zebra", Value(1));
        value.set("apple", Value(2));
        value.set("mango", Value(3));

        std::string order;

        for (const auto& member : value.members()) {
          order += member.first + " ";
        }

        expect(order).to_eq("zebra apple mango ");
      });

      spec::it("keeps its place when an existing key is replaced", [] {
        Value value = Value::object();
        value.set("first", Value(1));
        value.set("second", Value(2));
        value.set("first", Value(3));

        expect(value.members()[0].first).to_eq("first");
      });
    });

    spec::context("deep merge", [] {
      spec::it("adds keys the base does not have", [] {
        Value base = Value::object();
        base.set("a", Value(1));

        Value over = Value::object();
        over.set("b", Value(2));

        expect(Value::deep_merge(base, over)["b"].as_integer()).to_eq(std::int64_t{2});
      });

      spec::it("replaces a scalar", [] {
        Value base = Value::object();
        base.set("a", Value(1));

        Value over = Value::object();
        over.set("a", Value(2));

        expect(Value::deep_merge(base, over)["a"].as_integer()).to_eq(std::int64_t{2});
      });

      spec::it("merges nested objects rather than replacing them", [] {
        Value inner_base = Value::object();
        inner_base.set("keep", Value(1));

        Value base = Value::object();
        base.set("nested", inner_base);

        Value inner_over = Value::object();
        inner_over.set("add", Value(2));

        Value over = Value::object();
        over.set("nested", inner_over);

        const Value merged = Value::deep_merge(base, over);

        spec::aggregate_failures([&] {
          expect(merged["nested"]["keep"].as_integer()).to_eq(std::int64_t{1});
          expect(merged["nested"]["add"].as_integer()).to_eq(std::int64_t{2});
        });
      });

      spec::it("replaces an array rather than concatenating", [] {
        Value base = Value::object();
        base.set("list", Value::array({Value(1), Value(2)}));

        Value over = Value::object();
        over.set("list", Value::array({Value(3)}));

        expect(Value::deep_merge(base, over)["list"].size()).to_eq(std::size_t{1});
      });

      spec::it("replaces an object with a scalar when that is what arrives", [] {
        Value base = Value::object();
        base.set("thing", Value::object());

        Value over = Value::object();
        over.set("thing", Value("scalar"));

        expect(Value::deep_merge(base, over)["thing"].is_string()).to_be_true();
      });

      spec::it("returns the override when the base is not an object", [] {
        expect(Value::deep_merge(Value(1), Value(2)).as_integer()).to_eq(std::int64_t{2});
      });

      spec::it("leaves the base untouched", [] {
        Value base = Value::object();
        base.set("a", Value(1));

        Value over = Value::object();
        over.set("a", Value(2));

        Value::deep_merge(base, over);

        expect(base["a"].as_integer()).to_eq(std::int64_t{1});
      });
    });

    spec::context("size and emptiness", [] {
      spec::it("reports the length of a string", [] { expect(Value("abc").size()).to_eq(std::size_t{3}); });

      spec::it("reports nothing for a scalar", [] { expect(Value(1).size()).to_eq(std::size_t{0}); });

      spec::it("reports an empty container as empty", [] { expect(Value::array().empty()).to_be_true(); });

      spec::it("reports a filled container as not empty", [] {
        expect(Value::array({Value(1)}).empty()).to_be_false();
      });
    });

    spec::context("reading a container the wrong way", [] {
      spec::it("reads a key on a non-object as null", [] { expect(Value(1)["a"].is_null()).to_be_true(); });

      spec::it("finds nothing on a non-object", [] {
        expect(Value(1).find("a") == nullptr).to_be_true();
      });

      spec::it("reads an index on a non-array as null", [] { expect(Value(1).at(0).is_null()).to_be_true(); });

      spec::it("reports no members on a non-object", [] {
        expect(Value(1).members().empty()).to_be_true();
      });

      spec::it("reports no items on a non-array", [] { expect(Value(1).items().empty()).to_be_true(); });

      spec::it("becomes an object when set on", [] {
        Value value(1);
        value.set("a", Value(2));

        expect(value.is_object()).to_be_true();
      });
    });

    spec::context("naming types", [] {
      spec::it("names null", [] { expect(Value().type_name()).to_eq("null"); });

      spec::it("names a boolean", [] { expect(Value(true).type_name()).to_eq("boolean"); });

      spec::it("names a number", [] { expect(Value(1.5).type_name()).to_eq("number"); });

      spec::it("names a string", [] { expect(Value("a").type_name()).to_eq("string"); });

      spec::it("names an array", [] { expect(Value::array().type_name()).to_eq("array"); });

      spec::it("names an object", [] { expect(Value::object().type_name()).to_eq("object"); });
    });

    spec::context("comparison", [] {
      spec::it("treats equal scalars as equal", [] { expect(Value(1) == Value(1)).to_be_true(); });

      spec::it("treats different types as unequal", [] { expect(Value(1) == Value("1")).to_be_false(); });

      spec::it("compares objects by contents and order", [] {
        Value left = Value::object();
        left.set("a", Value(1));

        Value right = Value::object();
        right.set("a", Value(1));

        expect(left == right).to_be_true();
      });
    });
  });
}
