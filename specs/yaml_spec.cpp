#include <string>

#include "json.h"
#include "support/spec.h"
#include "value.h"
#include "yaml.h"

using blogin::parse_yaml;
using blogin::Value;
using spec::expect;

namespace {

Value parsed(std::string_view text) {
  return parse_yaml(text).value_or(Value());
}

bool rejects(std::string_view text) {
  return !parse_yaml(text).has_value();
}

std::string error_of(std::string_view text) {
  const auto result = parse_yaml(text);

  return result ? std::string("no error") : result.error().describe();
}

}  // namespace

SPEC {
  spec::describe("YAML", [] {
    spec::context("mappings", [] {
      spec::it("reads a key and value", [] {
        expect(std::string(parsed("title: Blogin")["title"].as_string())).to_eq("Blogin");
      });

      spec::it("reads several keys", [] {
        expect(parsed("a: 1\nb: 2\nc: 3").size()).to_eq(std::size_t{3});
      });

      spec::it("keeps keys in their original order", [] {
        expect(parsed("zebra: 1\napple: 2").members()[0].first).to_eq("zebra");
      });

      spec::it("reads a nested mapping", [] {
        expect(parsed("outer:\n  inner: value")["outer"]["inner"].is_string()).to_be_true();
      });

      spec::it("reads a mapping nested two deep", [] {
        expect(parsed("a:\n  b:\n    c: 1")["a"]["b"]["c"].as_integer()).to_eq(std::int64_t{1});
      });

      spec::it("reads a key with no value as null", [] {
        expect(parsed("a:\nb: 1")["a"].is_null()).to_be_true();
      });

      spec::it("returns an empty mapping for empty input", [] {
        expect(parsed("").is_object()).to_be_true();
      });
    });

    spec::context("scalars", [] {
      spec::it("reads an integer", [] { expect(parsed("n: 42")["n"].as_integer()).to_eq(std::int64_t{42}); });

      spec::it("reads a negative integer", [] {
        expect(parsed("n: -7")["n"].as_integer()).to_eq(std::int64_t{-7});
      });

      spec::it("reads a number", [] { expect(parsed("n: 2.5")["n"].as_number()).to_eq(2.5); });

      spec::it("reads true", [] { expect(parsed("flag: true")["flag"].as_boolean()).to_be_true(); });

      spec::it("reads false", [] { expect(parsed("flag: false")["flag"].is_boolean()).to_be_true(); });

      spec::it("reads null written as a tilde", [] { expect(parsed("n: ~")["n"].is_null()).to_be_true(); });

      spec::it("reads null written out", [] { expect(parsed("n: null")["n"].is_null()).to_be_true(); });

      spec::it("reads a plain string", [] {
        expect(std::string(parsed("s: hello world")["s"].as_string())).to_eq("hello world");
      });

      spec::it("keeps a version-like value a string", [] {
        expect(parsed("v: 1.2.3")["v"].is_string()).to_be_true();
      });
    });

    spec::context("quoted scalars", [] {
      spec::it("reads a double-quoted string", [] {
        expect(std::string(parsed("s: \"hello\"")["s"].as_string())).to_eq("hello");
      });

      spec::it("reads a single-quoted string", [] {
        expect(std::string(parsed("s: 'hello'")["s"].as_string())).to_eq("hello");
      });

      spec::it("keeps a quoted number a string", [] {
        expect(parsed("s: \"42\"")["s"].is_string()).to_be_true();
      });

      spec::it("reads an escape in a double-quoted string", [] {
        expect(std::string(parsed(R"(s: "a\nb")")["s"].as_string())).to_eq("a\nb");
      });

      spec::it("leaves an escape alone in a single-quoted string", [] {
        expect(std::string(parsed("s: 'a\\nb'")["s"].as_string())).to_eq("a\\nb");
      });

      spec::it("keeps a colon inside quotes", [] {
        expect(std::string(parsed("s: \"a: b\"")["s"].as_string())).to_eq("a: b");
      });
    });

    spec::context("sequences", [] {
      spec::it("reads a block sequence", [] {
        expect(parsed("items:\n  - one\n  - two").at(0).is_null()).to_be_true();
      });

      spec::it("reads block sequence items", [] {
        expect(parsed("items:\n  - one\n  - two")["items"].size()).to_eq(std::size_t{2});
      });

      spec::it("reads a sequence at the same indentation as its key", [] {
        expect(parsed("items:\n- one\n- two")["items"].size()).to_eq(std::size_t{2});
      });

      spec::it("reads an inline flow sequence", [] {
        expect(parsed("tags: [a, b, c]")["tags"].size()).to_eq(std::size_t{3});
      });

      spec::it("reads an empty flow sequence", [] {
        expect(parsed("tags: []")["tags"].size()).to_eq(std::size_t{0});
      });

      spec::it("reads quoted items in a flow sequence", [] {
        expect(std::string(parsed("tags: [\"a, b\", c]")["tags"].at(0).as_string())).to_eq("a, b");
      });

      spec::it("reads a top-level sequence", [] {
        expect(parsed("- one\n- two").size()).to_eq(std::size_t{2});
      });

      spec::it("reads a sequence of mappings", [] {
        const Value value = parsed("people:\n  - name: Greg\n    role: author\n  - name: Ada");

        expect(value["people"].size()).to_eq(std::size_t{2});
      });

      spec::it("reads every key of a mapping inside a sequence", [] {
        const Value value = parsed("people:\n  - name: Greg\n    role: author");

        spec::aggregate_failures([&] {
          expect(std::string(value["people"].at(0)["name"].as_string())).to_eq("Greg");
          expect(std::string(value["people"].at(0)["role"].as_string())).to_eq("author");
        });
      });

      spec::it("reads a sequence nested on its own lines", [] {
        expect(parsed("outer:\n  -\n    - a\n    - b")["outer"].at(0).size()).to_eq(std::size_t{2});
      });
    });

    spec::context("comments", [] {
      spec::it("ignores a whole-line comment", [] {
        expect(parsed("# a note\na: 1").size()).to_eq(std::size_t{1});
      });

      spec::it("ignores a trailing comment", [] {
        expect(parsed("a: 1 # a note")["a"].as_integer()).to_eq(std::int64_t{1});
      });

      spec::it("keeps a hash inside quotes", [] {
        expect(std::string(parsed("s: \"a # b\"")["s"].as_string())).to_eq("a # b");
      });

      spec::it("ignores blank lines", [] { expect(parsed("a: 1\n\n\nb: 2").size()).to_eq(std::size_t{2}); });

      spec::it("ignores a document marker", [] { expect(parsed("---\na: 1").size()).to_eq(std::size_t{1}); });
    });

    // Every escape a double-quoted scalar understands, and every shape a
    // sequence and a mapping can nest into. Each of these appears in a data
    // file somewhere.
    spec::context("more shapes", [] {
      spec::it("reads a newline escape", [] {
        expect(std::string(parsed(R"(a: "one\ntwo")")["a"].as_string())).to_eq("one\ntwo");
      });

      spec::it("reads a tab escape", [] {
        expect(std::string(parsed(R"(a: "one\ttwo")")["a"].as_string())).to_eq("one\ttwo");
      });

      spec::it("reads a carriage return escape", [] {
        expect(std::string(parsed(R"(a: "one\rtwo")")["a"].as_string())).to_eq("one\rtwo");
      });

      spec::it("reads an escaped quote", [] {
        expect(std::string(parsed(R"(a: "say \"hi\"")")["a"].as_string())).to_eq(R"(say "hi")");
      });

      spec::it("reads an escaped backslash", [] {
        expect(std::string(parsed(R"(a: "one\\two")")["a"].as_string())).to_eq(R"(one\two)");
      });

      spec::it("reads a quoted item in a flow sequence written with single quotes", [] {
        expect(std::string(parsed("a: ['one', 'two']")["a"].at(0).as_string())).to_eq("one");
      });

      spec::it("ignores an empty entry in a flow sequence", [] {
        expect(parsed("a: [one, , two]")["a"].size()).to_eq(std::size_t{2});
      });

      spec::it("keeps a colon inside a quoted value", [] {
        expect(std::string(parsed(R"(a: "one: two")")["a"].as_string())).to_eq("one: two");
      });

      spec::it("keeps a colon inside a single-quoted value", [] {
        expect(std::string(parsed("a: 'one: two'")["a"].as_string())).to_eq("one: two");
      });

      spec::it("reads a sequence at the same indentation as its key", [] {
        expect(parsed("items:\n- one\n- two")["items"].size()).to_eq(std::size_t{2});
      });

      spec::it("reads a dash on a line of its own as an empty entry", [] {
        expect(parsed("items:\n  -\n  - two")["items"].at(0).is_null()).to_be_true();
      });

      spec::it("reads a mapping nested under a dash on its own line", [] {
        expect(std::string(parsed("items:\n  -\n    name: one")["items"].at(0)["name"].as_string()))
          .to_eq("one");
      });

      spec::it("refuses a sequence of sequences", [] {
        expect(rejects("items:\n  - - one")).to_be_true();
      });

      spec::it("reads a lone minus sign as a string", [] {
        expect(std::string(parsed("a: -").as_string("")).empty()).to_be_true();
      });

      spec::it("reads a number with nothing after the point as a string", [] {
        expect(parsed("a: 1.")["a"].is_string()).to_be_true();
      });

      spec::it("keeps a colon inside a quoted key", [] {
        expect(parsed(R"("a: b": one)").members()[0].first).to_eq("a: b");
      });

      spec::it("reads a mapping whose first key sits on the dash line", [] {
        expect(std::string(parsed("items:\n  - name: one\n    role: two")["items"].at(0)["role"].as_string()))
          .to_eq("two");
      });

      spec::it("reads a key on the dash line with its value nested under it", [] {
        expect(std::string(parsed("items:\n  - name:\n      first: one")["items"].at(0)["name"]["first"]
                             .as_string()))
          .to_eq("one");
      });

      spec::it("reads a key on the dash line with no value at all", [] {
        expect(parsed("items:\n  - name:\n  - name: two")["items"].at(0)["name"].is_null()).to_be_true();
      });

      spec::it("refuses a sequence item where a mapping key belongs", [] {
        expect(rejects("a: 1\n- b")).to_be_true();
      });

      spec::it("refuses a bad escape in a key", [] {
        expect(rejects(R"("a\q": 1)")).to_be_true();
      });

      spec::it("refuses a bad escape in a flow sequence item", [] {
        expect(rejects(R"(a: ["b\q"])")).to_be_true();
      });

      spec::it("refuses a bad escape in a sequence item", [] {
        expect(rejects("items:\n  - \"b\\qc\"")).to_be_true();
      });

      spec::it("refuses a bad escape on a dash line's key", [] {
        expect(rejects("items:\n  - \"a\\qb\": one")).to_be_true();
      });

      spec::it("refuses a bad escape in a value nested under a key", [] {
        expect(rejects("a:\n  b: \"c\\qd\"")).to_be_true();
      });

      spec::it("refuses a bad escape in a value nested under a dash line's key", [] {
        expect(rejects("items:\n  - name:\n      first: \"a\\qb\"")).to_be_true();
      });

      spec::it("refuses a bad escape in a key continued past the dash", [] {
        expect(rejects("items:\n  - name: one\n    \"b\\qc\": two")).to_be_true();
      });

      spec::it("refuses a bad escape in a value on the dash line", [] {
        expect(rejects("items:\n  - name: \"a\\qb\"")).to_be_true();
      });

      spec::it("refuses a bad escape in a sequence written at its key's indentation", [] {
        expect(rejects("items:\n- \"a\\qb\"")).to_be_true();
      });

      spec::it("refuses a bad escape under a dash on its own line", [] {
        expect(rejects("items:\n  -\n    a: \"b\\qc\"")).to_be_true();
      });

      spec::it("refuses an item indented past the sequence it belongs to", [] {
        expect(rejects("items:\n  - one\n      - two")).to_be_true();
      });

      spec::it("stops a sequence at a key written at its own indentation", [] {
        expect(parsed("items:\n- one\nafter: 1").size()).to_eq(std::size_t{2});
      });

      spec::it("stops a sequence at a line that is not an item", [] {
        expect(parsed("items:\n  - one\nafter: 1").size()).to_eq(std::size_t{2});
      });
    });

    spec::context("what it refuses", [] {
      spec::it("refuses an anchor", [] { expect(rejects("a: &anchor value")).to_be_true(); });

      spec::it("refuses an alias", [] { expect(rejects("a: *anchor")).to_be_true(); });

      spec::it("refuses a flow mapping", [] { expect(rejects("a: {b: 1}")).to_be_true(); });

      spec::it("refuses a block scalar", [] { expect(rejects("a: |")).to_be_true(); });

      spec::it("refuses a folded scalar", [] { expect(rejects("a: >")).to_be_true(); });

      spec::it("refuses an unknown escape", [] { expect(rejects(R"(a: "b\qc")")).to_be_true(); });

      // The alternative silently yields the string "- a", not a sequence.
      spec::it("refuses a nested sequence packed onto the dash line", [] {
        expect(rejects("outer:\n  - - a\n    - b")).to_be_true();
      });

      spec::it("refuses a line that is not a mapping or a sequence item", [] {
        expect(rejects("a: 1\njust text")).to_be_true();
      });

      spec::it("refuses unexpected indentation", [] { expect(rejects("a: 1\n    b: 2")).to_be_true(); });

      spec::it("names the line it refused", [] { expect(error_of("a: 1\n\njust text")).to_contain("line 3"); });

      spec::it("says what it does not support", [] {
        expect(error_of("a: &anchor value")).to_contain("anchors");
      });
    });

    // The same shape the fuzzer found in the expression parser. Every nesting
    // level recurses, so a deeply indented file has to be refused before the
    // stack runs out.
    spec::context("nesting", [] {
      const auto nested = [](int depth) {
        std::string out;

        for (int level = 0; level < depth; ++level) {
          out += std::string(static_cast<std::size_t>(level) * 2, ' ') + "k:\n";
        }

        return out;
      };

      spec::it("reads what a data file would plausibly hold", [=] {
        expect(blogin::parse_yaml(nested(10)).has_value()).to_be_true();
      });

      spec::it("refuses nesting past the limit rather than crashing", [=] {
        expect(blogin::parse_yaml(nested(400)).error().message).to_contain("nested too deeply");
      });

      // Found by the fuzzer. Data files are written as JSON on the way
      // to the search index, so anything read here has to survive being written
      // there and read back. How many levels of JSON a level here becomes
      // depends on the shape: a mapping is one, a sequence of mappings is two,
      // an array holding an object. So the shapes below each reach the limit at
      // a different indentation, and every one of them has to round-trip.
      spec::context("the deepest file it accepts", [=] {
        // Asking the parser where its limit is, so nothing here drifts when
        // the limit moves.
        const auto deepest_accepted = [](const auto& build) {
          int deepest = 0;

          for (int depth = 1; depth <= 500; ++depth) {
            if (!blogin::parse_yaml(build(depth))) {
              break;
            }

            deepest = depth;
          }

          return deepest;
        };

        const auto round_trips = [=](const auto& build) {
          const auto source = build(deepest_accepted(build));
          const auto parsed = blogin::parse_yaml(source);

          return parsed && blogin::parse_json(blogin::to_json(*parsed)).has_value();
        };

        const auto indent = [](int level) { return std::string(static_cast<std::size_t>(level) * 2, ' '); };

        const auto sequence_of_mappings = [=](int depth) {
          std::string out;

          for (int level = 0; level < depth; ++level) {
            out += indent(level) + "- k:\n";
          }

          return out;
        };

        // What the fuzzer landed on: neither shape alone, so a fixed ratio
        // between the two limits held for both of those and not for this.
        const auto mappings_and_sequences = [=](int depth) {
          std::string out;

          for (int level = 0; level < depth; ++level) {
            out += indent(level) + (level % 2 == 0 ? "- k:\n" : "k:\n");
          }

          return out;
        };

        spec::it("can be written as JSON and read back when it is a sequence of mappings", [=] {
          expect(round_trips(sequence_of_mappings)).to_be_true();
        });

        spec::it("can be written as JSON and read back when it is all mappings", [=] {
          expect(round_trips(nested)).to_be_true();
        });

        spec::it("can be written as JSON and read back when it mixes the two", [=] {
          expect(round_trips(mappings_and_sequences)).to_be_true();
        });
      });
    });
  });
}
