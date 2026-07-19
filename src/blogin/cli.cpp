#include "cli.h"

#include <array>
#include <charconv>
#include <format>
#include <map>

namespace blogin::cli {
namespace {

struct Verbs {
  std::string_view name;
  Verb verb;
  bool takes_subject;
  std::string_view subject_name;
};

constexpr Verbs verbs[] = {
  {"build", Verb::build, false, {}},
  {"init", Verb::init, true, "directory"},
  {"new", Verb::new_post, true, "title"},
  {"serve", Verb::serve, false, {}},
  {"clean", Verb::clean, false, {}},
};

const Verbs* find_verb(std::string_view name) {
  for (const Verbs& candidate : verbs) {
    if (candidate.name == name) {
      return &candidate;
    }
  }

  return nullptr;
}

// One short spelling per option, unique across every command so a flag means the
// same thing wherever it appears. Combining them into one argument, as in -fv,
// is not read: an option is one argument.
struct Short {
  char letter;
  std::string_view name;

  // An option taking a value can only be the last letter of a combined run,
  // since the value is the next argument and there is only one of those.
  bool takes_value;
};

constexpr std::array<Short, 13> short_options{{
  {'s', "src", true},
  {'o', "out", true},
  {'S', "section", true},
  {'w', "framework", true},
  {'j', "jobs", true},
  {'p', "port", true},
  {'d', "drafts", false},
  {'F', "future", false},
  {'f', "force", false},
  {'D', "debug", false},
  {'N', "no-debug", false},
  {'c', "counters", false},
  {'v', "verbose", false},
}};

const Short* short_option_for(char letter) {
  for (const Short& option : short_options) {
    if (option.letter == letter) {
      return &option;
    }
  }

  return nullptr;
}

ParseError error(std::string message) {
  return ParseError{std::move(message), 1, 1};
}

std::expected<int, ParseError> to_number(std::string_view flag, std::string_view value) {
  int number = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), number);

  if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
    return std::unexpected(error(std::format("--{} wants a number, not '{}'", flag, value)));
  }

  return number;
}

}  // namespace

std::string version() {
#ifdef BLOGIN_VERSION
  return BLOGIN_VERSION;
#else
  return "unknown";
#endif
}

std::string usage() {
  return "usage: blogin build [--src <dir>] [--out <dir>] [--drafts] [--future] [--jobs <n>]\n"
         "                    [--debug] [--no-debug] [--force] [--counters] [--verbose]\n"
         "       blogin init [<dir>] [--framework none|bootstrap5|pico|bulma] [--force] [--verbose]\n"
         "       blogin new <title> [--src <dir>] [--section <name>] [--force] [--verbose]\n"
         "       blogin serve [--src <dir>] [--port <n>] [--verbose]\n"
         "       blogin clean [--out <dir>] [--verbose]\n"
         "\n"
         "\n"
         "  blogin --version, -V   what version this is\n"
         "\n"
         "every option has a short spelling, and they combine, as in -fv:\n"
         "  -s --src        -o --out        -S --section    -w --framework\n"
         "  -j --jobs       -p --port       -d --drafts     -F --future\n"
         "  -f --force      -D --debug      -N --no-debug   -c --counters\n"
         "  -v --verbose\n";
}

std::expected<Command, ParseError> parse(const std::vector<std::string>& arguments) {
  if (arguments.empty()) {
    return std::unexpected(error("no command given"));
  }

  // Asking what version this is answers on its own, so it is read before a verb
  // is looked for and needs none.
  for (const std::string& argument : arguments) {
    if (argument == "--version" || argument == "-V") {
      Command command;
      command.verb = Verb::version;

      return command;
    }
  }

  const Verbs* verb = find_verb(arguments.front());

  if (verb == nullptr) {
    return std::unexpected(error(std::format("unknown command '{}'", arguments.front())));
  }

  Command command;
  command.verb = verb->verb;

  bool has_subject = false;

  // Flags are read wherever they appear, so `blogin new --section notes "A
  // title"` and `blogin new "A title" --section notes` are the same command.
  for (std::size_t index = 1; index < arguments.size(); ++index) {
    const std::string& argument = arguments[index];

    // Short options resolve to their long names and then take the same path, so
    // the two spellings cannot drift apart. A run like -fv is several options in
    // one argument, and each letter is resolved in turn.
    std::vector<std::string> resolved;

    if (argument.size() > 1 && argument[0] == '-' && argument[1] != '-') {
      const std::string_view letters(argument.begin() + 1, argument.end());

      for (std::size_t position = 0; position < letters.size(); ++position) {
        const Short* option = short_option_for(letters[position]);

        if (option == nullptr) {
          return std::unexpected(error(std::format("unknown option '-{}' in '{}'",
                                                   letters[position], argument)));
        }

        // The value is the next argument, and there is only one of those, so an
        // option wanting one has to be the last letter of the run.
        if (option->takes_value && position + 1 != letters.size()) {
          return std::unexpected(
            error(std::format("'-{}' wants a value, so it has to come last in '{}'",
                              letters[position], argument)));
        }

        resolved.emplace_back(option->name);
      }
    }

    if (resolved.empty() && !argument.starts_with("--")) {
      if (!verb->takes_subject) {
        return std::unexpected(
          error(std::format("'{}' takes no arguments, but got '{}'", verb->name, argument)));
      }

      if (has_subject) {
        return std::unexpected(error(std::format("'{}' takes one {}, but got '{}' as well",
                                                 verb->name, verb->subject_name, argument)));
      }

      command.subject = argument;
      has_subject = true;

      continue;
    }

    if (resolved.empty()) {
      resolved.emplace_back(argument.begin() + 2, argument.end());
    }

    for (const std::string& name : resolved) {
      const std::string_view flag(name);

      // A value flag takes the next argument, which has to be there. The message
      // names the option the way it was written.
      const auto value = [&](std::string_view) -> std::expected<std::string, ParseError> {
        if (index + 1 >= arguments.size()) {
          return std::unexpected(error(std::format("{} wants a value", argument)));
        }

        return arguments[++index];
      };

        if (flag == "src") {
        auto given = value(flag);

        if (!given) {
          return std::unexpected(given.error());
        }

        command.content = *given;
      } else if (flag == "out") {
        auto given = value(flag);

        if (!given) {
          return std::unexpected(given.error());
        }

        command.output = *given;
      } else if (flag == "section") {
        auto given = value(flag);

        if (!given) {
          return std::unexpected(given.error());
        }

        command.section = *given;
      } else if (flag == "framework") {
        auto given = value(flag);

        if (!given) {
          return std::unexpected(given.error());
        }

        command.framework = *given;
      } else if (flag == "jobs" || flag == "port") {
        auto given = value(flag);

        if (!given) {
          return std::unexpected(given.error());
        }

        auto number = to_number(flag, *given);

        if (!number) {
          return std::unexpected(number.error());
        }

        if (flag == "jobs") {
          command.jobs = *number;
        } else {
          command.port = *number;
        }
      } else if (flag == "drafts") {
        command.drafts = true;
      } else if (flag == "future") {
        command.future = true;
      } else if (flag == "force") {
        command.force = true;
      } else if (flag == "verbose") {
        command.verbose = true;
      } else if (flag == "counters") {
        command.counters = true;
      } else if (flag == "debug") {
        command.debug = true;
      } else if (flag == "no-debug") {
        command.debug = false;
      } else {
        return std::unexpected(error(std::format("unknown option '{}'", argument)));
      }
    }
  }

  if (verb->takes_subject && !has_subject) {
    if (verb->verb == Verb::init) {
      command.subject = ".";
    } else {
      return std::unexpected(error(std::format("'{}' wants a {}", verb->name, verb->subject_name)));
    }
  }

  return command;
}

}  // namespace blogin::cli
