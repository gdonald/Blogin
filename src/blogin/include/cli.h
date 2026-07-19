#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "json.h"

namespace blogin::cli {

enum class Verb {
  build,
  version,
  init,
  new_post,
  serve,
  clean,
};

// What the command line asked for, with nothing acted on yet.
//
// Parsing is separate from doing so that a spec can ask what a command line
// means without a filesystem, a site, or a subprocess.
struct Command {
  Verb verb = Verb::build;

  // The positional argument, if the verb takes one: the directory for `init`,
  // the title for `new`.
  std::string subject;

  std::string content = "content";
  std::optional<std::string> output;
  std::optional<std::string> section;
  std::string framework = "none";

  std::optional<int> jobs;
  int port = 3000;

  bool drafts = false;
  bool future = false;
  bool force = false;
  bool verbose = false;
  bool counters = false;
  std::optional<bool> debug;
};

// What `blogin --version` prints, from the version project() was given.
std::string version();

// Reads a command line, without the program name.
//
// An unknown flag, a missing value, or a missing positional argument is an
// error naming what was wrong, never a default quietly standing in for it.
std::expected<Command, ParseError> parse(const std::vector<std::string>& arguments);

// What to print when the command line does not parse.
std::string usage();

}  // namespace blogin::cli
