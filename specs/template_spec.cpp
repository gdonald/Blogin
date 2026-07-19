#include <string>

#include "counters.h"
#include "support/spec.h"
#include "template.h"

using blogin::CompiledTemplate;
using blogin::Context;
using spec::expect;

namespace {

std::string render(std::string source, const Context& context) {
  return render_template(CompiledTemplate::compile(std::move(source)), context);
}

std::string render(std::string source) {
  const Context context;

  return render(std::move(source), context);
}

}  // namespace

SPEC {
  spec::describe("templates", [] {
    spec::context("elements", [] {
      spec::it("renders an empty element", [] { expect(render("%p")).to_eq("<p></p>\n"); });

      spec::it("nests by indentation", [] {
        expect(render("%div\n  %p")).to_eq("<div><p></p>\n</div>\n");
      });

      spec::it("closes a nested element when indentation returns", [] {
        expect(render("%div\n  %p\n%footer")).to_eq("<div><p></p>\n</div>\n<footer></footer>\n");
      });

      spec::it("renders text written after a tag", [] { expect(render("%p hello")).to_eq("<p>hello</p>\n"); });

      spec::it("ignores blank lines between elements", [] {
        expect(render("%div\n\n  %p")).to_eq("<div><p></p>\n</div>\n");
      });
    });

    spec::context("substitution", [] {
      auto context = spec::let([] {
        Context values;
        values.set("title", "Blogin");
        values.set("markup", "<em>hi</em>");
        values.set("unsafe", "a < b");

        return values;
      });

      spec::it("writes a context value", [=] {
        expect(render("%h1= title", context())).to_eq("<h1>Blogin</h1>\n");
      });

      spec::it("escapes a value written with the escaping form", [=] {
        expect(render("%h1= unsafe", context())).to_eq("<h1>a &lt; b</h1>\n");
      });

      spec::it("leaves a value written with the raw form unescaped", [=] {
        expect(render("%div!= markup", context())).to_eq("<div><em>hi</em></div>\n");
      });

      spec::it("writes nothing for an unknown name", [=] {
        expect(render("%p= missing", context())).to_eq("<p></p>\n");
      });
    });

    spec::context("yield", [] {
      spec::it("splices the body in place", [] {
        Context context;
        context.set_body("<p>post</p>\n");

        expect(render("%main\n  = yield", context)).to_eq("<main><p>post</p>\n</main>\n");
      });
    });

    spec::context("compiling once", [] {
      // Reads the process-global work counters.
      spec::serial();

      spec::before_each([] { blogin::reset_counters(); });

      spec::it("compiles a template once however many times it renders", [] {
        const CompiledTemplate compiled = CompiledTemplate::compile("%p= title");
        const Context context;

        for (int index = 0; index < 25; ++index) {
          render_template(compiled, context);
        }

        spec::aggregate_failures([] {
          expect(blogin::counter_value(blogin::Counter::templates_compiled)).to_eq(std::uint64_t{1});
          expect(blogin::counter_value(blogin::Counter::pages_rendered)).to_eq(std::uint64_t{25});
        });
      });
    });
  });
}
