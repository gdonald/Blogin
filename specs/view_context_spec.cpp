#include <string>
#include <vector>

#include "support/spec.h"
#include "view_context.h"

using blogin::Value;
using blogin::ViewContext;
using spec::expect;

namespace {

ViewContext sample_context() {
  ViewContext context;

  context.set("title", Value("Blogin"));
  context.define("url", [](const std::vector<ViewContext::Argument>&) { return Value("/here"); });

  return context;
}

}  // namespace

// The surface a layout reads from. Everything the expression language resolves
// goes through here, and what a fragment read is recorded here too, which is
// what lets a cache key be derived rather than declared.
SPEC {
  spec::describe("the view context", [] {
    spec::context("names", [] {
      spec::it("offers a name it was given", [] {
        expect(sample_context().has("title")).to_be_true();
      });

      spec::it("offers a function as a name too", [] {
        expect(sample_context().has("url")).to_be_true();
      });

      spec::it("offers nothing it was never given", [] {
        expect(sample_context().has("subtitle")).to_be_false();
      });

      spec::it("replaces a value set twice rather than keeping both", [] {
        ViewContext context = sample_context();
        context.set("title", Value("Second"));

        expect(std::string(context.lookup("title")->as_string())).to_eq("Second");
      });

      spec::it("replaces a function defined twice", [] {
        ViewContext context = sample_context();
        context.define("url", [](const std::vector<ViewContext::Argument>&) { return Value("/there"); });

        expect(std::string((*context.function("url"))({}).as_string())).to_eq("/there");
      });

      spec::it("says a name it has is callable", [] {
        expect(sample_context().callable("url")).to_be_true();
      });

      spec::it("says a plain value is not callable", [] {
        expect(sample_context().callable("title")).to_be_false();
      });
    });

    // A loop variable and a partial's arguments are locals, which come and go
    // with the body they belong to.
    spec::context("locals", [] {
      spec::it("shadows a view name", [] {
        ViewContext context = sample_context();
        context.set_local("title", Value("Local"));

        expect(std::string(context.lookup_local("title")->as_string())).to_eq("Local");
      });

      spec::it("replaces a local set twice", [] {
        ViewContext context = sample_context();
        context.set_local("entry", Value("first"));
        context.set_local("entry", Value("second"));

        expect(std::string(context.lookup_local("entry")->as_string())).to_eq("second");
      });

      spec::it("knows it has one", [] {
        ViewContext context = sample_context();
        context.set_local("entry", Value("x"));

        expect(context.has_local("entry")).to_be_true();
      });

      spec::it("knows when it has none by that name", [] {
        expect(sample_context().has_local("entry")).to_be_false();
      });

      spec::it("gives them all back and takes them again", [] {
        ViewContext context = sample_context();
        context.set_local("entry", Value("x"));

        auto saved = context.take_locals();

        const bool cleared = !context.has_local("entry");

        context.restore_locals(std::move(saved));

        spec::aggregate_failures([&] {
          expect(cleared).to_be_true();
          expect(context.has_local("entry")).to_be_true();
        });
      });
    });

    spec::context("recording what a fragment read", [] {
      spec::it("is not recording until it is told to", [] {
        expect(sample_context().recording()).to_be_false();
      });

      spec::it("records nothing when nothing asked it to", [] {
        expect(sample_context().end_recording()).to_eq("");
      });

      spec::it("records a name that was read", [] {
        ViewContext context = sample_context();

        context.begin_recording();
        context.lookup("title");

        expect(context.end_recording()).to_contain("title");
      });

      spec::it("names what it read in the order it read it", [] {
        ViewContext context = sample_context();

        context.begin_recording();
        context.lookup("title");
        context.end_recording();

        expect(context.read_names()).to_eq(std::vector<std::string>{"title"});
      });

      // A loop variable follows from the collection, which is recorded, so it
      // is not a dependency of its own.
      spec::it("leaves out a name the fragment bound itself", [] {
        ViewContext context = sample_context();

        context.begin_recording();
        context.set_local("entry", Value("x"));
        context.lookup_local("entry");
        context.end_recording();

        expect(context.read_names()).to_eq(std::vector<std::string>{});
      });

      spec::it("replays the same names against the same values", [] {
        ViewContext context = sample_context();

        context.begin_recording();
        context.lookup("title");

        const std::string key = context.end_recording();

        expect(context.replay(context.read_names()).value()).to_eq(key);
      });

      spec::it("replays a local as readily as a view name", [] {
        ViewContext context = sample_context();
        context.set_local("entry", Value("x"));

        expect(context.replay({"entry"}).has_value()).to_be_true();
      });

      // Nothing to replay against sends the caller back to rendering.
      spec::it("replays nothing for a name no longer in scope", [] {
        expect(sample_context().replay({"gone"}).has_value()).to_be_false();
      });

      spec::it("is replayable when every name held still", [] {
        ViewContext context = sample_context();

        context.begin_recording();
        context.lookup("title");
        context.end_recording();

        expect(context.replayable()).to_be_true();
      });

      // Resolving the name once now would speak for only one of the two values
      // the render saw, so this fragment is rendered rather than predicted.
      spec::it("is not replayable when a name came out twice over", [] {
        ViewContext context = sample_context();

        context.begin_recording();
        context.lookup("title");
        context.set("title", Value("Changed"));
        context.lookup("title");
        context.end_recording();

        expect(context.replayable()).to_be_false();
      });
    });
  });
}
