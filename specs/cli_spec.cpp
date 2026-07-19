#include <algorithm>
#include <string>
#include <vector>

#include "cli.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::expected<blogin::cli::Command, blogin::ParseError> parse(const std::vector<std::string>& arguments) {
  return blogin::cli::parse(arguments);
}


// Two parsed command lines mean the same thing. Compared field by field rather
// than through an operator on Command, since nothing in the library needs one.
bool same_command(const blogin::cli::Command& left, const blogin::cli::Command& right) {
  return left.verb == right.verb && left.subject == right.subject &&
         left.content == right.content && left.output == right.output &&
         left.section == right.section && left.framework == right.framework &&
         left.jobs == right.jobs && left.port == right.port && left.drafts == right.drafts &&
         left.future == right.future && left.force == right.force &&
         left.verbose == right.verbose && left.counters == right.counters &&
         left.debug == right.debug;
}

}  // namespace

SPEC {
  spec::describe("the command line", [] {
    spec::context("choosing a command", [] {
      spec::it("reads build", [] { expect(parse({"build"})->verb == blogin::cli::Verb::build).to_be_true(); });

      spec::it("reads init", [] { expect(parse({"init"})->verb == blogin::cli::Verb::init).to_be_true(); });

      spec::it("reads new", [] {
        expect(parse({"new", "A Title"})->verb == blogin::cli::Verb::new_post).to_be_true();
      });

      spec::it("reads serve", [] { expect(parse({"serve"})->verb == blogin::cli::Verb::serve).to_be_true(); });

      spec::it("reads clean", [] { expect(parse({"clean"})->verb == blogin::cli::Verb::clean).to_be_true(); });

      spec::it("refuses a command it does not know", [] {
        expect(parse({"publish"}).error().message).to_contain("unknown command 'publish'");
      });

      spec::it("refuses an empty command line", [] {
        expect(parse({}).error().message).to_contain("no command");
      });
    });

    spec::context("defaults", [] {
      spec::it("builds from content", [] { expect(parse({"build"})->content).to_eq("content"); });

      spec::it("initialises the current directory", [] { expect(parse({"init"})->subject).to_eq("."); });

      spec::it("starts a site with no framework", [] { expect(parse({"init"})->framework).to_eq("none"); });

      spec::it("serves on port 3000", [] { expect(parse({"serve"})->port).to_eq(3000); });

      spec::it("leaves the output directory to the configuration", [] {
        expect(parse({"build"})->output.has_value()).to_be_false();
      });

      spec::it("leaves debug to the configuration", [] {
        expect(parse({"build"})->debug.has_value()).to_be_false();
      });
    });

    spec::context("flags", [] {
      spec::it("reads a value flag", [] {
        expect(parse({"build", "--src", "essays"})->content).to_eq("essays");
      });

      spec::it("reads a boolean flag", [] { expect(parse({"build", "--drafts"})->drafts).to_be_true(); });

      spec::it("reads a number", [] { expect(parse({"build", "--jobs", "4"})->jobs.value()).to_eq(4); });

      spec::it("turns debug on", [] { expect(parse({"build", "--debug"})->debug.value()).to_be_true(); });

      // Without this there is no way to override a configuration that has it on.
      spec::it("turns debug off", [] {
        expect(parse({"build", "--no-debug"})->debug.value()).to_be_false();
      });

      spec::it("refuses a flag it does not know", [] {
        expect(parse({"build", "--fast"}).error().message).to_contain("unknown option '--fast'");
      });

      spec::it("refuses a value flag with nothing after it", [] {
        expect(parse({"build", "--src"}).error().message).to_contain("--src wants a value");
      });

      spec::it("refuses a number that is not one", [] {
        expect(parse({"build", "--jobs", "many"}).error().message).to_contain("wants a number");
      });

      spec::it("reads where to write the output", [] {
        expect(parse({"build", "--out", "site"})->output.value()).to_eq("site");
      });

      spec::it("reads the framework to scaffold with", [] {
        expect(parse({"init", "here", "--framework", "bootstrap5"})->framework).to_eq("bootstrap5");
      });

      spec::it("reads the port to serve on", [] {
        expect(parse({"serve", "--port", "8123"})->port).to_eq(8123);
      });

      spec::it("reads the section to write into", [] {
        expect(parse({"new", "A Title", "--section", "notes"})->section.value()).to_eq("notes");
      });

      spec::it("turns future posts on", [] {
        expect(parse({"build", "--future"})->future).to_be_true();
      });

      spec::it("forces the work through", [] {
        expect(parse({"build", "--force"})->force).to_be_true();
      });

      spec::it("asks for a running commentary", [] {
        expect(parse({"build", "--verbose"})->verbose).to_be_true();
      });

      spec::it("asks for the counters", [] {
        expect(parse({"build", "--counters"})->counters).to_be_true();
      });

      const std::vector<std::string> value_flags{"out", "section", "framework", "jobs", "port"};

      for (const std::string& flag : value_flags) {
        spec::it("refuses --" + flag + " with nothing after it", [flag] {
          expect(parse({"new", "--" + flag}).error().message).to_contain("--" + flag + " wants a value");
        });
      }

      spec::it("refuses a port that is not a number", [] {
        expect(parse({"serve", "--port", "soon"}).error().message).to_contain("wants a number");
      });
    });

    spec::it("says how it is used", [] { expect(blogin::cli::usage()).to_contain("usage: blogin build"); });

    // A flag means the same thing wherever it appears, so nobody has to
    // remember an order.
    spec::context("flags anywhere", [] {
      spec::it("reads a flag written before the title", [] {
        expect(parse({"new", "--section", "notes", "A Title"})->subject).to_eq("A Title");
      });

      spec::it("reads a flag written after the title", [] {
        expect(parse({"new", "A Title", "--section", "notes"})->section.value()).to_eq("notes");
      });
    });

    spec::context("positional arguments", [] {
      spec::it("reads the directory to initialise", [] {
        expect(parse({"init", "site"})->subject).to_eq("site");
      });

      spec::it("wants a title for a new post", [] {
        expect(parse({"new"}).error().message).to_contain("wants a title");
      });

      spec::it("refuses a second positional argument", [] {
        expect(parse({"init", "one", "two"}).error().message).to_contain("as well");
      });

      spec::it("refuses an argument to a command that takes none", [] {
        expect(parse({"build", "content"}).error().message).to_contain("takes no arguments");
      });
    });
  });

  // Every option has a short spelling. A short one resolves to its long name
  // and then takes the same path, so the table below is what keeps the two
  // from drifting apart.
  spec::describe("short options", [] {
    struct Pair {
      std::string shorter;
      std::string longer;
      std::string value;
    };

    const std::vector<Pair> pairs{
      {"-s", "--src", "elsewhere"}, {"-o", "--out", "dist"},   {"-S", "--section", "notes"},
      {"-w", "--framework", "bootstrap5"}, {"-j", "--jobs", "3"}, {"-p", "--port", "4000"},
      {"-d", "--drafts", ""},       {"-F", "--future", ""},    {"-f", "--force", ""},
      {"-D", "--debug", ""},        {"-N", "--no-debug", ""},  {"-c", "--counters", ""},
      {"-v", "--verbose", ""},
    };

    // Whichever verb accepts the option, so each pair is compared on a command
    // that actually takes it.
    const auto verb_for = [](const std::string& longer) -> std::vector<std::string> {
      if (longer == "--section") {
        return {"new", "A Title"};
      }

      if (longer == "--framework") {
        return {"init", "site"};
      }

      if (longer == "--port") {
        return {"serve"};
      }

      return {"build"};
    };

    spec::it("reads the same command as the long spelling", [=] {
      std::vector<std::string> differing;

      for (const Pair& pair : pairs) {
        std::vector<std::string> shorter = verb_for(pair.longer);
        std::vector<std::string> longer = shorter;

        shorter.push_back(pair.shorter);
        longer.push_back(pair.longer);

        if (!pair.value.empty()) {
          shorter.push_back(pair.value);
          longer.push_back(pair.value);
        }

        const auto a = blogin::cli::parse(shorter);
        const auto b = blogin::cli::parse(longer);

        if (!a || !b || !same_command(*a, *b)) {
          differing.push_back(pair.shorter + " against " + pair.longer);
        }
      }

      expect(differing).to_eq(std::vector<std::string>{});
    });

    spec::it("gives every option a short spelling", [=] {
      expect(pairs.size()).to_eq(std::size_t{13});
    });

    // Combining them into one argument is the usual convention, so -fv is -f -v.
    spec::context("combined into one argument", [] {
      spec::it("reads two booleans as both", [] {
        const auto combined = blogin::cli::parse({"build", "-fv"});
        const auto apart = blogin::cli::parse({"build", "-f", "-v"});

        expect(same_command(*combined, *apart)).to_be_true();
      });

      spec::it("reads a longer run", [] {
        const auto combined = blogin::cli::parse({"build", "-dFcv"});
        const auto apart = blogin::cli::parse({"build", "-d", "-F", "-c", "-v"});

        expect(same_command(*combined, *apart)).to_be_true();
      });

      // The value is the next argument, so an option wanting one can end a run.
      spec::it("lets a value option come last", [] {
        const auto combined = blogin::cli::parse({"build", "-fo", "dist"});
        const auto apart = blogin::cli::parse({"build", "-f", "-o", "dist"});

        expect(same_command(*combined, *apart)).to_be_true();
      });

      spec::it("keeps the value that followed the run", [] {
        const auto parsed = blogin::cli::parse({"build", "-fo", "dist"});

        expect(parsed->output.value_or("")).to_eq("dist");
      });
    });

    spec::context("refusing", [] {
      spec::it("names a short option it does not know", [] {
        const auto parsed = blogin::cli::parse({"build", "-Z"});

        expect(parsed.error().message).to_contain("unknown option '-Z'");
      });

      spec::it("names the letter and the run it was found in", [] {
        const auto parsed = blogin::cli::parse({"build", "-fZv"});

        spec::aggregate_failures([&] {
          expect(parsed.error().message).to_contain("'-Z'");
          expect(parsed.error().message).to_contain("-fZv");
        });
      });

      // Only one argument follows a run, so only one option in it can want it.
      spec::it("refuses a value option before another letter", [] {
        const auto parsed = blogin::cli::parse({"build", "-of", "dist"});

        expect(parsed.error().message).to_contain("has to come last");
      });

      spec::it("names the spelling that was written when a value is missing", [] {
        const auto parsed = blogin::cli::parse({"build", "-o"});

        expect(parsed.error().message).to_contain("-o wants a value");
      });

      // A path can begin with a dash. It is the value of the option before it,
      // not a run of letters.
      spec::it("reads a value beginning with a dash as the value", [] {
        const auto parsed = blogin::cli::parse({"build", "-o", "-weird"});

        expect(parsed->output.value_or("")).to_eq("-weird");
      });
    });
  });

  spec::describe("asking for the version", [] {
    spec::it("reads --version", [] {
      expect(blogin::cli::parse({"--version"})->verb == blogin::cli::Verb::version).to_be_true();
    });

    spec::it("reads -V", [] {
      expect(blogin::cli::parse({"-V"})->verb == blogin::cli::Verb::version).to_be_true();
    });

    // It answers on its own, so it needs no command in front of it and wins
    // over one that is there.
    spec::it("answers even after a command", [] {
      expect(blogin::cli::parse({"build", "--version"})->verb == blogin::cli::Verb::version)
        .to_be_true();
    });

    // The value comes from project() in CMakeLists, so a release cannot report
    // a version other than the one it was built from.
    spec::it("reports a version rather than nothing", [] {
      expect(blogin::cli::version()).not_to_eq("");
    });

    spec::it("reports three dot-separated parts", [] {
      const std::string reported = blogin::cli::version();

      expect(std::count(reported.begin(), reported.end(), '.')).to_eq(std::ptrdiff_t{2});
    });
  });
}
