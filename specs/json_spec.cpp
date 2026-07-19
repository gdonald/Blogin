#include <string>

#include "json.h"
#include "support/spec.h"
#include "value.h"

using blogin::JsonStyle;
using blogin::parse_json;
using blogin::to_json;
using blogin::Value;
using spec::expect;

namespace {

Value parsed(std::string_view text) {
  return parse_json(text).value_or(Value());
}

std::string error_of(std::string_view text) {
  const auto result = parse_json(text);

  return result ? std::string("no error") : result.error().describe();
}

bool rejects(std::string_view text) {
  return !parse_json(text).has_value();
}

}  // namespace

SPEC {
  spec::describe("JSON", [] {
    spec::context("parsing scalars", [] {
      spec::it("reads null", [] { expect(parsed("null").is_null()).to_be_true(); });

      spec::it("reads true", [] { expect(parsed("true").as_boolean()).to_be_true(); });

      spec::it("reads false", [] { expect(parsed("false").as_boolean(true)).to_be_false(); });

      spec::it("reads an integer", [] { expect(parsed("42").as_integer()).to_eq(std::int64_t{42}); });

      spec::it("reads a negative integer", [] { expect(parsed("-7").as_integer()).to_eq(std::int64_t{-7}); });

      spec::it("reads a number with a fraction", [] { expect(parsed("2.5").as_number()).to_eq(2.5); });

      spec::it("reads a number with an exponent", [] { expect(parsed("2e3").as_number()).to_eq(2000.0); });

      spec::it("keeps an integer an integer", [] { expect(parsed("42").is_integer()).to_be_true(); });

      spec::it("makes a fractional value a number", [] { expect(parsed("42.0").is_number()).to_be_true(); });

      spec::it("reads a string", [] { expect(std::string(parsed("\"text\"").as_string())).to_eq("text"); });

      spec::it("reads an empty string", [] { expect(parsed("\"\"").is_string()).to_be_true(); });
    });

    spec::context("parsing escapes", [] {
      spec::it("reads a quote", [] {
        expect(std::string(parsed(R"("a\"b")").as_string())).to_eq("a\"b");
      });

      spec::it("reads a backslash", [] {
        expect(std::string(parsed(R"("a\\b")").as_string())).to_eq("a\\b");
      });

      spec::it("reads a newline", [] { expect(std::string(parsed(R"("a\nb")").as_string())).to_eq("a\nb"); });

      spec::it("reads a tab", [] { expect(std::string(parsed(R"("a\tb")").as_string())).to_eq("a\tb"); });

      spec::it("reads a solidus", [] { expect(std::string(parsed(R"("a\/b")").as_string())).to_eq("a/b"); });

      spec::it("reads a basic-plane code point", [] {
        expect(std::string(parsed(R"("é")").as_string())).to_eq("é");
      });

      spec::it("reads a surrogate pair as one character", [] {
        expect(std::string(parsed(R"("😀")").as_string())).to_eq("\U0001F600");
      });

      spec::it("passes utf-8 through untouched", [] {
        expect(std::string(parsed("\"café\"").as_string())).to_eq("café");
      });
    });

    spec::context("parsing containers", [] {
      spec::it("reads an empty array", [] { expect(parsed("[]").is_array()).to_be_true(); });

      spec::it("reads array items", [] { expect(parsed("[1,2,3]").size()).to_eq(std::size_t{3}); });

      spec::it("reads a nested array", [] {
        expect(parsed("[[1]]").at(0).at(0).as_integer()).to_eq(std::int64_t{1});
      });

      spec::it("reads an empty object", [] { expect(parsed("{}").is_object()).to_be_true(); });

      spec::it("reads object members", [] {
        expect(parsed(R"({"a":1})")["a"].as_integer()).to_eq(std::int64_t{1});
      });

      spec::it("reads a nested object", [] {
        expect(parsed(R"({"a":{"b":2}})")["a"]["b"].as_integer()).to_eq(std::int64_t{2});
      });

      spec::it("keeps members in their original order", [] {
        const Value value = parsed(R"({"zebra":1,"apple":2})");

        expect(value.members()[0].first).to_eq("zebra");
      });

      spec::it("ignores whitespace between tokens", [] {
        expect(parsed("  {\n  \"a\" : 1\n }  ")["a"].as_integer()).to_eq(std::int64_t{1});
      });
    });

    spec::context("rejecting malformed input", [] {
      spec::it("rejects an empty document", [] { expect(rejects("")).to_be_true(); });

      spec::it("rejects a trailing comma in an object", [] {
        expect(rejects(R"({"a":1,})")).to_be_true();
      });

      spec::it("rejects a trailing comma in an array", [] { expect(rejects("[1,]")).to_be_true(); });

      spec::it("rejects an unquoted key", [] { expect(rejects("{a:1}")).to_be_true(); });

      spec::it("rejects a single-quoted string", [] { expect(rejects("'text'")).to_be_true(); });

      spec::it("rejects an unterminated string", [] { expect(rejects("\"open")).to_be_true(); });

      spec::it("rejects an unterminated object", [] { expect(rejects(R"({"a":1)")).to_be_true(); });

      spec::it("rejects an unterminated array", [] { expect(rejects("[1")).to_be_true(); });

      spec::it("rejects a missing colon", [] { expect(rejects(R"({"a" 1})")).to_be_true(); });

      spec::it("rejects trailing content", [] { expect(rejects("{} {}")).to_be_true(); });

      spec::it("rejects a bare word", [] { expect(rejects("undefined")).to_be_true(); });

      spec::it("rejects a leading plus", [] { expect(rejects("+1")).to_be_true(); });

      spec::it("rejects a bare decimal point", [] { expect(rejects("1.")).to_be_true(); });

      spec::it("rejects an empty exponent", [] { expect(rejects("1e")).to_be_true(); });

      spec::it("rejects an unescaped control character", [] { expect(rejects("\"a\nb\"")).to_be_true(); });

      spec::it("rejects an unknown escape", [] { expect(rejects(R"("a\qb")")).to_be_true(); });

      spec::it("rejects an incomplete unicode escape", [] { expect(rejects(R"("\u00")")).to_be_true(); });

      // A deeply nested document would otherwise recurse until the stack runs
      // out, which is a crash rather than an error message.
      spec::it("rejects a document nested past the depth limit", [] {
        expect(rejects(std::string(500, '[') + std::string(500, ']'))).to_be_true();
      });

      spec::it("accepts nesting a real document might reach", [] {
        expect(rejects(std::string(20, '[') + std::string(20, ']'))).to_be_false();
      });
    });

    spec::context("reporting where an error is", [] {
      spec::it("names the line", [] { expect(error_of("{\n  \"a\" 1\n}")).to_contain("line 2"); });

      spec::it("names the column", [] { expect(error_of("{\n  \"a\" 1\n}")).to_contain("column"); });

      spec::it("says what went wrong", [] {
        expect(error_of(R"({"a":1,})")).to_contain("trailing comma");
      });

      spec::it("includes the path when given one", [] {
        const auto result = parse_json("{");

        expect(result.error().describe("blogin.json")).to_start_with("blogin.json:");
      });
    });

    spec::context("emitting", [] {
      spec::it("writes null", [] { expect(to_json(Value())).to_eq("null"); });

      spec::it("writes a boolean", [] { expect(to_json(Value(true))).to_eq("true"); });

      spec::it("writes an integer", [] { expect(to_json(Value(42))).to_eq("42"); });

      spec::it("writes a string", [] { expect(to_json(Value("text"))).to_eq("\"text\""); });

      spec::it("writes an empty array", [] { expect(to_json(Value::array())).to_eq("[]"); });

      spec::it("writes an empty object", [] { expect(to_json(Value::object())).to_eq("{}"); });

      spec::it("writes array items", [] {
        expect(to_json(Value::array({Value(1), Value(2)}))).to_eq("[1,2]");
      });

      spec::it("escapes a quote", [] { expect(to_json(Value("a\"b"))).to_eq(R"("a\"b")"); });

      spec::it("escapes a newline", [] { expect(to_json(Value("a\nb"))).to_eq(R"("a\nb")"); });

      spec::it("escapes a control character as a code point", [] {
        expect(to_json(Value(std::string("a\x01" "b")))).to_eq(R"("a\u0001b")");
      });

      spec::it("leaves utf-8 as bytes rather than escaping it", [] {
        expect(to_json(Value("café"))).to_eq("\"café\"");
      });

      spec::it("writes members in their original order", [] {
        Value value = Value::object();
        value.set("zebra", Value(1));
        value.set("apple", Value(2));

        expect(to_json(value)).to_eq(R"({"zebra":1,"apple":2})");
      });

      // The search index is emitted with sorted keys so the file is stable
      // whatever order posts were discovered in.
      spec::it("sorts members when asked", [] {
        Value value = Value::object();
        value.set("zebra", Value(1));
        value.set("apple", Value(2));

        expect(to_json(value, JsonStyle::compact, true)).to_eq(R"({"apple":2,"zebra":1})");
      });

      spec::it("indents when pretty", [] {
        Value value = Value::object();
        value.set("a", Value(1));

        expect(to_json(value, JsonStyle::pretty)).to_eq("{\n  \"a\": 1\n}\n");
      });

      spec::it("indents nested containers", [] {
        Value inner = Value::object();
        inner.set("b", Value(1));

        Value outer = Value::object();
        outer.set("a", inner);

        expect(to_json(outer, JsonStyle::pretty)).to_eq("{\n  \"a\": {\n    \"b\": 1\n  }\n}\n");
      });
    });

    spec::context("emitting the harder cases", [] {
      spec::it("writes a number", [] { expect(to_json(Value(2.5))).to_eq("2.5"); });

      spec::it("writes a negative number", [] { expect(to_json(Value(-0.25))).to_eq("-0.25"); });

      spec::it("writes a nested array pretty", [] {
        expect(to_json(Value::array({Value(1), Value(2)}), JsonStyle::pretty)).to_eq("[\n  1,\n  2\n]\n");
      });

      spec::it("writes an array of objects pretty", [] {
        Value member = Value::object();
        member.set("a", Value(1));

        expect(to_json(Value::array({member}), JsonStyle::pretty)).to_eq("[\n  {\n    \"a\": 1\n  }\n]\n");
      });

      spec::it("writes an empty array pretty without a blank line", [] {
        expect(to_json(Value::array(), JsonStyle::pretty)).to_eq("[]\n");
      });

      spec::it("writes an empty object pretty without a blank line", [] {
        expect(to_json(Value::object(), JsonStyle::pretty)).to_eq("{}\n");
      });

      spec::it("sorts keys inside a nested object too", [] {
        Value inner = Value::object();
        inner.set("z", Value(1));
        inner.set("a", Value(2));

        Value outer = Value::object();
        outer.set("nested", inner);

        expect(to_json(outer, JsonStyle::compact, true)).to_eq(R"({"nested":{"a":2,"z":1}})");
      });

      spec::it("escapes a tab", [] { expect(to_json(Value("a\tb"))).to_eq(R"("a\tb")"); });

      spec::it("escapes a carriage return", [] { expect(to_json(Value("a\rb"))).to_eq(R"("a\rb")"); });

      spec::it("escapes a backspace", [] { expect(to_json(Value("a\bb"))).to_eq(R"("a\bb")"); });

      spec::it("escapes a form feed", [] { expect(to_json(Value("a\fb"))).to_eq(R"("a\fb")"); });

      spec::it("escapes a backslash", [] { expect(to_json(Value("a\\b"))).to_eq(R"("a\\b")"); });
    });

    spec::context("more malformed input", [] {
      spec::it("rejects a truncated true", [] { expect(rejects("tru")).to_be_true(); });

      spec::it("rejects a truncated false", [] { expect(rejects("fals")).to_be_true(); });

      spec::it("rejects a truncated null", [] { expect(rejects("nul")).to_be_true(); });

      spec::it("rejects a lone minus", [] { expect(rejects("-")).to_be_true(); });

      spec::it("rejects an unterminated escape", [] { expect(rejects("\"a\\")).to_be_true(); });

      spec::it("rejects a bad hex digit in an escape", [] { expect(rejects(R"("\u00zz")")).to_be_true(); });

      spec::it("rejects an object ending after a key", [] { expect(rejects(R"({"a")")).to_be_true(); });

      spec::it("rejects an object with a missing value", [] { expect(rejects(R"({"a":})")).to_be_true(); });

      spec::it("rejects whitespace alone", [] { expect(rejects("   ")).to_be_true(); });

      spec::it("reads a lone high surrogate as its own character", [] {
        expect(parsed(R"("\ud800")").is_string()).to_be_true();
      });

      spec::it("reads a high surrogate followed by a non-surrogate", [] {
        expect(parsed(R"("\ud800\u0041")").is_string()).to_be_true();
      });
    });

    spec::context("round trips", [] {
      spec::it("preserves a document through parse and emit", [] {
        const std::string original = R"({"title":"Blogin","tags":["a","b"],"count":3,"deep":{"ok":true}})";

        expect(to_json(parsed(original))).to_eq(original);
      });

      spec::it("preserves escapes", [] {
        const std::string original = R"({"text":"a\nb\"c"})";

        expect(to_json(parsed(original))).to_eq(original);
      });

      spec::it("preserves an empty container", [] {
        expect(to_json(parsed(R"({"a":[],"b":{}})"))).to_eq(R"({"a":[],"b":{}})");
      });

      spec::context("a number that reads as a negative zero", [] {
        // Writing "-0" reads back as the integer zero, which then writes as
        // "0", so a document emitted twice used to come out different both
        // times. A build that caches its output and compares it notices.
        spec::it("writes it as a plain zero", [] {
          expect(to_json(parsed("[-0.0]"))).to_eq("[0]");
        });

        spec::it("writes the same bytes on a second pass", [] {
          const std::string once = to_json(parsed("[-0e105]"));

          expect(to_json(parsed(once))).to_eq(once);
        });
      });
    });
  });
}
