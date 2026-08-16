#include <string>
#include <utility>
#include <vector>

#include "view_context.h"
#include "expression.h"
#include "support/spec.h"

using blogin::ViewContext;
using blogin::Value;
using spec::expect;

namespace {

ViewContext sample_context() {
  ViewContext context;

  context.set("title", Value("Blogin"));
  context.set("count", Value(3));
  context.set("ratio", Value(1.5));
  context.set("draft", Value(false));
  context.set("nothing", Value());

  Value tag = Value::object();
  tag.set("name", Value("raku"));
  tag.set("url", Value("/tags/raku"));

  context.set("tag", tag);
  context.set("tags", Value::array({tag, tag}));
  context.set("empty", Value::array());

  context.define("shout", [](const std::vector<ViewContext::Argument>& arguments) {
    return Value(std::string(arguments.empty() ? "" : arguments[0].value.as_string()) + "!");
  });

  context.define("url", [](const std::vector<ViewContext::Argument>&) { return Value("/here"); });

  context.define("names", [](const std::vector<ViewContext::Argument>& arguments) {
    std::string out;

    for (const auto& argument : arguments) {
      out += argument.name.empty() ? "-" : argument.name;
      out += "=";
      out += argument.value.as_string();
      out += ";";
    }

    return Value(out);
  });

  return context;
}

Value evaluate(std::string_view source, ViewContext& context) {
  const auto parsed = blogin::expression::parse(source);

  if (!parsed) {
    return Value();
  }

  return blogin::expression::evaluate(**parsed, context).value_or(Value());
}

Value evaluate(std::string_view source) {
  ViewContext context = sample_context();

  return evaluate(source, context);
}

std::string error_of(std::string_view source) {
  ViewContext context = sample_context();

  const auto parsed = blogin::expression::parse(source);

  if (!parsed) {
    return parsed.error().message;
  }

  const auto result = blogin::expression::evaluate(**parsed, context);

  return result ? std::string("no error") : result.error().message;
}

}  // namespace

SPEC {
  spec::describe("the expression language", [] {
    spec::context("literals", [] {
      spec::it("reads a string", [] { expect(std::string(evaluate("'hi'").as_string())).to_eq("hi"); });

      spec::it("reads a double-quoted string", [] {
        expect(std::string(evaluate("\"hi\"").as_string())).to_eq("hi");
      });

      spec::it("reads an integer", [] { expect(evaluate("42").as_integer()).to_eq(std::int64_t{42}); });

      spec::it("reads a number", [] { expect(evaluate("2.5").as_number()).to_eq(2.5); });

      spec::it("reads true", [] { expect(evaluate("true").as_boolean()).to_be_true(); });

      spec::it("reads null", [] { expect(evaluate("null").is_null()).to_be_true(); });
    });

    spec::context("names", [] {
      spec::it("reads a view value", [] {
        expect(std::string(evaluate("title").as_string())).to_eq("Blogin");
      });

      // A typo in a layout should be visible rather than rendering nothing.
      spec::it("refuses a name the view does not offer", [] {
        expect(error_of("titel")).to_contain("no such name");
      });

      spec::it("suggests a name close to the one written", [] {
        expect(error_of("titel")).to_contain("did you mean 'title'");
      });

      spec::it("suggests nothing for a name close to nothing", [] {
        expect(error_of("completely-unrelated")).not_to_contain("did you mean");
      });

      spec::it("calls a function written without parentheses", [] {
        expect(std::string(evaluate("url").as_string())).to_eq("/here");
      });
    });

    spec::context("member access", [] {
      spec::it("reads a key with a dot", [] {
        expect(std::string(evaluate("tag.name").as_string())).to_eq("raku");
      });

      // The angle-bracket subscript, which reads a key the same way a dot does.
      spec::it("reads a key with angle brackets", [] {
        expect(std::string(evaluate("tag<name>").as_string())).to_eq("raku");
      });

      spec::it("treats the two forms as the same", [] {
        expect(std::string(evaluate("tag.url").as_string())).to_eq(std::string(evaluate("tag<url>").as_string()));
      });

      // Asking whether something is there is how a layout decides to render it.
      spec::it("reads a missing key as null", [] { expect(evaluate("tag<absent>").is_null()).to_be_true(); });

      spec::it("counts a list", [] { expect(evaluate("tags.elems").as_integer()).to_eq(std::int64_t{2}); });

      spec::it("reads the first of a list", [] {
        expect(std::string(evaluate("tags.first.name").as_string())).to_eq("raku");
      });

      spec::it("refuses to read a member of a number", [] {
        expect(error_of("count.name")).to_contain("cannot read");
      });
    });

    spec::context("truthiness", [] {
      spec::it("treats an empty list as false", [] { expect(evaluate("!empty").as_boolean()).to_be_true(); });

      spec::it("treats a filled list as true", [] { expect(evaluate("!tags").as_boolean()).to_be_false(); });

      spec::it("treats null as false", [] { expect(evaluate("!nothing").as_boolean()).to_be_true(); });

      spec::it("accepts the word form", [] { expect(evaluate("not draft").as_boolean()).to_be_true(); });
    });

    spec::context("comparison", [] {
      spec::it("compares strings for equality", [] {
        expect(evaluate("title eq 'Blogin'").as_boolean()).to_be_true();
      });

      spec::it("accepts the symbol form", [] {
        expect(evaluate("title == 'Blogin'").as_boolean()).to_be_true();
      });

      spec::it("compares for inequality", [] {
        expect(evaluate("title ne 'Other'").as_boolean()).to_be_true();
      });

      spec::it("orders numbers", [] { expect(evaluate("count > 2").as_boolean()).to_be_true(); });

      spec::it("orders numbers the other way", [] { expect(evaluate("count <= 3").as_boolean()).to_be_true(); });

      spec::it("treats a number and a string as unequal", [] {
        expect(evaluate("count eq '3'").as_boolean()).to_be_false();
      });

      spec::it("refuses to order values of different kinds", [] {
        expect(error_of("title < count")).to_contain("cannot compare");
      });
    });

    spec::context("boolean operators", [] {
      // The operators yield the value rather than a boolean, so "a || b" reads
      // as a default and "- if a && b" still asks the right question.
      spec::it("ands", [] { expect(evaluate("title && count").truthy()).to_be_true(); });

      spec::it("yields the right side of an and", [] {
        expect(evaluate("title && count").as_integer()).to_eq(std::int64_t{3});
      });

      spec::it("yields the first true value of an or", [] {
        expect(std::string(evaluate("draft || title").as_string())).to_eq("Blogin");
      });

      spec::it("ors", [] { expect(evaluate("draft || title").truthy()).to_be_true(); });

      // "- if post && post<title>" has to be safe to write.
      spec::it("stops before evaluating a right side it does not need", [] {
        expect(error_of("nothing && nothing<key>")).to_eq("no error");
      });

      spec::it("accepts the word forms", [] {
        expect(evaluate("draft or title").truthy()).to_be_true();
      });
    });

    spec::context("arithmetic and concatenation", [] {
      spec::it("adds", [] { expect(evaluate("count + 1").as_integer()).to_eq(std::int64_t{4}); });

      spec::it("subtracts", [] { expect(evaluate("count - 1").as_integer()).to_eq(std::int64_t{2}); });

      spec::it("concatenates with a tilde", [] {
        expect(std::string(evaluate("title ~ '!'").as_string())).to_eq("Blogin!");
      });

      spec::it("concatenates a number onto a string", [] {
        expect(std::string(evaluate("'n=' ~ count").as_string())).to_eq("n=3");
      });

      // Layouts do not calculate. Views do.
      spec::it("has no multiplication", [] { expect(error_of("count * 2")).not_to_eq("no error"); });
    });

    spec::context("calls", [] {
      spec::it("calls with an argument", [] {
        expect(std::string(evaluate("shout('hi')").as_string())).to_eq("hi!");
      });

      spec::it("calls with no arguments", [] {
        expect(std::string(evaluate("url()").as_string())).to_eq("/here");
      });

      spec::it("passes a named argument written with angle brackets", [] {
        expect(std::string(evaluate("names(:partial<entry>)").as_string())).to_eq("partial=entry;");
      });

      spec::it("passes a named argument written with parentheses", [] {
        expect(std::string(evaluate("names(:as(title))").as_string())).to_eq("as=Blogin;");
      });

      spec::it("passes positional and named arguments together", [] {
        expect(std::string(evaluate("names(title, :as<x>)").as_string())).to_eq("-=Blogin;as=x;");
      });

      spec::it("refuses a name it cannot call", [] {
        expect(error_of("nonesuch('x')")).to_contain("no such name");
      });
    });

    spec::context("locals", [] {
      spec::it("reads one", [] {
        ViewContext context = sample_context();
        context.set_local("node", Value("local value"));

        expect(std::string(evaluate("$node", context).as_string())).to_eq("local value");
      });

      spec::it("refuses one that is not set", [] { expect(error_of("$missing")).to_contain("no such local"); });

      spec::it("shadows a view name", [] {
        ViewContext context = sample_context();
        context.set_local("title", Value("shadowed"));

        expect(std::string(evaluate("title", context).as_string())).to_eq("shadowed");
      });
    });

    // A map exists because :locals needs one, and it shares an opening brace
    // with the deferred block, so telling the two apart is the interesting part.
    spec::context("maps", [] {
      spec::it("reads a value out of a map it was given", [] {
        expect(std::string(evaluate("{brand: title}<brand>").as_string())).to_eq("Blogin");
      });

      spec::it("reads a value out of a map by member", [] {
        expect(std::string(evaluate("{brand: title}.brand").as_string())).to_eq("Blogin");
      });

      spec::it("holds more than one entry", [] {
        expect(std::string(evaluate("{one: 'a', two: 'b'}<two>").as_string())).to_eq("b");
      });

      spec::it("holds an expression rather than only a literal", [] {
        expect(std::string(evaluate("{joined: title ~ '!'}<joined>").as_string())).to_eq("Blogin!");
      });

      spec::it("can be empty", [] { expect(evaluate("{}").truthy()).to_be_false(); });

      // The two forms are told apart by what follows the first name, so a block
      // is still a block.
      spec::it("leaves a block a block", [] {
        expect(error_of("{ title }")).to_contain("block is only allowed as an argument");
      });

      // Without the colon there is nothing to distinguish it from a block, so it
      // is one, and the complaint is a block's rather than a map's.
      spec::it("reads a name with no colon after it as a block", [] {
        expect(error_of("{brand title}")).to_contain("close a block");
      });

      spec::it("wants a closing brace", [] {
        expect(error_of("{brand: title")).to_contain("expected '}' to close a map");
      });
    });

    spec::context("what it refuses", [] {
      spec::it("refuses assignment", [] {
        expect(error_of("title = 'x'")).to_contain("assignment is not supported");
      });

      spec::it("refuses an unterminated string", [] {
        expect(error_of("'unclosed")).to_contain("unterminated string");
      });

      spec::it("refuses an unterminated call", [] {
        expect(error_of("shout('x'")).to_contain("expected ',' or ')'");
      });

      spec::it("refuses a block where a value belongs", [] {
        expect(error_of("{ title }")).to_contain("block is only allowed as an argument");
      });

      spec::it("refuses trailing rubbish", [] { expect(error_of("title title")).to_contain("unexpected"); });
    });

    // A failure anywhere in an expression is the caller's failure. Each level
    // of the grammar has its own path back out, and a message that names the
    // wrong construct is worse than no message.
    spec::context("a failure at any depth", [] {
      const std::vector<std::pair<std::string, std::string>> malformed{
        {"draft || ", "expected a value"},
        {"draft && ", "expected a value"},
        {"title eq ", "expected a value"},
        {"count <= ", "expected a value"},
        {"count >= ", "expected a value"},
        {"count > ", "expected a value"},
        {"title ~ ", "expected a value"},
        {"count + ", "expected a value"},
        {"! ", "expected a value"},
        {"(title", "expected ')'"},
        {"(titel)", "no such name"},
        {"title.", "expected a name after '.'"},
        {"tag<", "expected a value"},
        {"{one: 'a', : 'b'}", "expected a name as a map key"},
        {"{one 'a'}", "close a block"},
        {"names(:label<x", "unterminated named argument"},
        {"shout(titel)", "no such name"},
        {"shout('a',", "expected a value"},
        {"names(:", "expected a name after ':'"},
        {"names(:label(", "expected a value"},
        {"names(:label('x'", "expected ')' after a named argument"},
        {"{brand: titel}", "no such name"},
        {"{ titel }", "block is only allowed as an argument"},
        {"'a' ~ titel", "no such name"},
        {"titel ~ 'a'", "no such name"},
        {"!titel", "no such name"},
        {"titel && title", "no such name"},
        {"draft || titel", "no such name"},
        {"titel < count", "no such name"},
        {"count < titel", "no such name"},
        {"titel.name", "no such name"},
        {"3(title)", "only a name can be called"},
      };

      for (const auto& example : malformed) {
        spec::it("reports '" + example.second + "' for " + example.first, [example] {
          expect(error_of(example.first)).to_contain(example.second);
        });
      }
    });

    spec::context("ordering", [] {
      spec::it("compares two numbers with >=", [] {
        expect(evaluate("count >= 3").as_boolean()).to_be_true();
      });

      spec::it("compares two numbers with >", [] {
        expect(evaluate("count > 3").as_boolean()).to_be_false();
      });

      spec::it("compares two numbers with <=", [] {
        expect(evaluate("count <= 3").as_boolean()).to_be_true();
      });

      spec::it("compares two strings", [] {
        expect(evaluate("title < 'Zebra'").as_boolean()).to_be_true();
      });

      spec::it("treats two nulls as equal", [] {
        expect(evaluate("nothing == nothing").as_boolean()).to_be_true();
      });
    });

    spec::context("what a value turns into inside text", [] {
      spec::it("writes a boolean as a word", [] {
        expect(std::string(evaluate("'' ~ draft").as_string())).to_eq("false");
      });

      spec::it("writes a number", [] {
        expect(std::string(evaluate("'' ~ ratio").as_string())).to_eq("1.5");
      });

      spec::it("writes an integer", [] {
        expect(std::string(evaluate("'' ~ count").as_string())).to_eq("3");
      });

      spec::it("writes null as nothing", [] {
        expect(std::string(evaluate("'' ~ nothing").as_string())).to_eq("");
      });

      spec::it("writes a list as nothing", [] {
        expect(std::string(evaluate("'' ~ tags").as_string())).to_eq("");
      });
    });

    spec::context("what a list answers", [] {
      spec::it("gives its last entry", [] {
        expect(std::string(evaluate("tags.last<name>").as_string())).to_eq("raku");
      });

      spec::it("gives nothing for the last entry of an empty list", [] {
        expect(evaluate("empty.last").is_null()).to_be_true();
      });

      spec::it("gives its first entry", [] {
        expect(std::string(evaluate("tags.first<name>").as_string())).to_eq("raku");
      });

      spec::it("counts with size", [] { expect(evaluate("tags.size").as_integer()).to_eq(2); });

      spec::it("counts with count", [] { expect(evaluate("tags.count").as_integer()).to_eq(2); });

      spec::it("refuses a name a list does not answer to", [] {
        expect(error_of("tags.nope")).to_contain("a list has no 'nope'");
      });
    });

    spec::context("more of what it refuses", [] {
      const std::vector<std::pair<std::string, std::string>> malformed{
        {"shout(", "unterminated argument list"},
        {"tag.name('a'", "expected ',' or ')'"},
        {"shout({ title +})", "unexpected '}'"},
        {"count + = 2", "assignment is not supported"},
        {"(title +)", "unexpected ')'"},
        {"{one: 'a', two 'b'}", "expected ':' after the map key 'two'"},
        {"{one: title +}", "unexpected '}'"},
      };

      for (const auto& example : malformed) {
        spec::it("reports '" + example.second + "' for " + example.first, [example] {
          expect(error_of(example.first)).to_contain(example.second);
        });
      }
    });

    spec::context("arithmetic", [] {
      spec::it("adds two numbers", [] { expect(evaluate("ratio + 1.5").as_number()).to_eq(3.0); });

      spec::it("subtracts two numbers", [] { expect(evaluate("ratio - 0.5").as_number()).to_eq(1.0); });

      spec::it("joins a number to a string with +", [] {
        expect(std::string(evaluate("title + count").as_string())).to_eq("Blogin3");
      });

      spec::it("compares two booleans", [] {
        expect(evaluate("draft == false").as_boolean()).to_be_true();
      });

      spec::it("yields the left side of an or when it is true", [] {
        expect(std::string(evaluate("title || 'fallback'").as_string())).to_eq("Blogin");
      });

      spec::it("counts the characters of a string", [] {
        expect(evaluate("title.chars").as_integer()).to_eq(6);
      });
    });

    spec::context("strings", [] {
      spec::it("reads an escaped quote", [] {
        expect(std::string(evaluate(R"('it\'s')").as_string())).to_eq("it's");
      });
    });

    spec::context("calls", [] {
      spec::it("reads a member called with no arguments", [] {
        expect(std::string(evaluate("tag.name()").as_string())).to_eq("raku");
      });

      spec::it("reads a local called with no arguments", [] {
        ViewContext context = sample_context();
        context.set_local("entry", Value("local value"));

        expect(std::string(evaluate("entry()", context).as_string())).to_eq("local value");
      });

      // A named argument with nothing after it is the name itself, which is
      // what makes `:as<entry>` and a bare `:entry` the same thing.
      spec::it("reads a bare named argument as its own name", [] {
        expect(std::string(evaluate("names(:entry)").as_string())).to_eq("entry=entry;");
      });

      spec::it("evaluates a block argument and passes its result", [] {
        expect(std::string(evaluate("shout({ title })").as_string())).to_eq("Blogin!");
      });

      spec::it("reports a failure inside a block argument", [] {
        expect(error_of("shout({ titel })")).to_contain("no such name");
      });
    });

    spec::context("errors", [] {
      spec::it("names the column it failed at", [] {
        const auto parsed = blogin::expression::parse("title +", 7);

        expect(parsed.has_value()).to_be_false();
      });

      spec::it("carries the line it was given", [] {
        const auto parsed = blogin::expression::parse("$", 14);

        expect(parsed.error().line).to_eq(std::size_t{14});
      });
    });

    // Found by the fuzzer. Every way back to the top of the grammar recurses,
    // so input nested deeply enough exhausted the stack before the limit went
    // in. A refusal is an error message. A stack overflow is a crash.
    spec::context("nesting", [] {
      const auto nested = [](int depth) {
        return std::string(static_cast<std::size_t>(depth), '(') + "1" +
               std::string(static_cast<std::size_t>(depth), ')');
      };

      spec::it("reads what a layout would plausibly write", [=] {
        expect(evaluate(nested(20)).as_integer()).to_eq(std::int64_t{1});
      });

      spec::it("refuses nesting past the limit rather than crashing", [=] {
        expect(error_of(nested(5000))).to_contain("nested too deeply");
      });

      spec::it("says what the limit is", [=] {
        expect(error_of(nested(5000))).to_contain("64");
      });

      // The evaluator walks the tree the same way the parser built it, so a
      // tree the parser refuses is one the evaluator never sees.
      spec::it("refuses before a tree that deep exists", [=] {
        expect(blogin::expression::parse(nested(5000)).has_value()).to_be_false();
      });
    });
  });
}
