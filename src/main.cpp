#include <algorithm>
#include <chrono>
#include <exception>
#include <expected>
#include <format>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <atomic>
#include <csignal>

#include "cli.h"
#include "config.h"
#include "counters.h"
#include "files.h"
#include "log.h"
#include "reload.h"
#include "scaffold.h"
#include "server.h"
#include "site.h"

namespace {

blogin::Log log_for(const blogin::cli::Command& command) {
  return blogin::Log(command.verbose ? blogin::LogLevel::verbose : blogin::LogLevel::normal);
}

int fail(const std::string& message) {
  std::cerr << "blogin: " << message << "\n";

  return 1;
}

std::filesystem::path root_of(const std::filesystem::path& content) {
  return content.parent_path().empty() ? "." : content.parent_path();
}

// Loads the site's configuration and says what is wrong with it rather than
// carrying on with defaults.
std::expected<blogin::Config, std::string> configuration(const std::filesystem::path& root) {
  const std::filesystem::path path = root / "blogin.json";

  auto config = blogin::Config::load(path);

  if (!config) {
    return std::unexpected(config.error().describe(path.string()));
  }

  for (const std::string& unknown : config->unknown_keys) {
    std::cerr << "blogin: unknown config key '" << unknown << "'";

    if (const std::string hint = blogin::nearest_key_hint(unknown); !hint.empty()) {
      std::cerr << " " << hint;
    }

    std::cerr << "\n";
  }

  return *config;
}

int run_build(const blogin::cli::Command& command) {
  const std::filesystem::path content = command.content;

  auto config = configuration(root_of(content));

  if (!config) {
    return fail(config.error());
  }

  if (command.debug.has_value()) {
    config->debug = *command.debug;
  }

  blogin::BuildOptions options = blogin::BuildOptions::around(content, *config);

  if (command.output.has_value()) {
    options.output = *command.output;
  }

  options.drafts = command.drafts;
  options.future = command.future;
  options.force = command.force;

  if (command.jobs.has_value()) {
    options.jobs = std::max(1, *command.jobs);
  }

  const blogin::Log log = log_for(command);

  log.verbose(std::format("content   {}", options.content.string()));
  log.verbose(std::format("layouts   {}", options.layouts.string()));
  log.verbose(std::format("output    {}", options.output.string()));

  const auto started = std::chrono::steady_clock::now();

  auto report = blogin::build_site(options);

  if (!report) {
    return fail(report.error().describe());
  }

  for (const std::string& warning : report->warnings) {
    std::cerr << "blogin: " << warning << "\n";
  }

  const auto elapsed =
    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started);

  std::cout << report->pages << " pages (" << report->rendered << " rendered), " << report->listings
            << " listings, " << report->written << " written, " << report->skipped << " unchanged, "
            << report->fragments_reused << " fragments reused (" << report->fragments_rendered
            << " rendered) in " << static_cast<int>(elapsed.count()) << "ms\n";

  if (command.counters || command.verbose) {
    std::cout << blogin::counters_report();
  }

  return 0;
}

int run_init(const blogin::cli::Command& command) {
  auto made = blogin::scaffold::init(command.subject, command.framework, command.force,
                                     blogin::scaffold::today());

  if (!made) {
    return fail(made.error().message);
  }

  std::cout << "scaffolded a Blogin site in " << made->string() << "\n";

  const blogin::Log log = log_for(command);

  for (const std::filesystem::path& file : blogin::files::all_files(*made)) {
    log.verbose(std::format("  {}", std::filesystem::relative(file, *made).generic_string()));
  }

  return 0;
}

int run_new(const blogin::cli::Command& command) {
  const std::filesystem::path content = command.content;

  auto config = configuration(root_of(content));

  if (!config) {
    return fail(config.error());
  }

  const std::string section = command.section.value_or(config->home_section);

  auto made = blogin::scaffold::new_post(command.subject, content, section, blogin::scaffold::today(),
                                         command.force);

  if (!made) {
    return fail(made.error().message);
  }

  std::cout << "created " << made->string() << "\n";

  return 0;
}

int run_clean(const blogin::cli::Command& command) {
  const std::filesystem::path root = root_of(command.content);

  auto config = configuration(root);

  if (!config) {
    return fail(config.error());
  }

  // A named target is the only one, since asking for one directory and getting
  // another emptied as well is not what was asked for.
  if (command.output.has_value()) {
    const std::filesystem::path target(*command.output);

    auto removed = blogin::clean(target, root);

    if (!removed) {
      return fail(removed.error().message);
    }

    std::cout << "cleaned " << target.string() << " (" << *removed << " files)\n";

    return 0;
  }

  // Otherwise both trees a build can write: the configured output and the one
  // the preview server uses. Leaving the preview behind would mean a cleaned
  // site still had a stale copy of itself on disk.
  const std::filesystem::path output = blogin::BuildOptions::around(command.content, *config).output;

  for (const std::filesystem::path& target : {output, root / blogin::reload::preview_output_dir}) {
    if (!std::filesystem::exists(target)) {
      continue;
    }

    auto removed = blogin::clean(target, root);

    if (!removed) {
      return fail(removed.error().message);
    }

    std::cout << "cleaned " << target.string() << " (" << *removed << " files)\n";
  }

  return 0;
}

// The running server, so a signal handler can stop it. A handler may touch
// almost nothing, and an atomic pointer is one of the few things it may.
std::atomic<blogin::Server*> running{nullptr};

void handle_interrupt(int /*signal*/) {
  if (blogin::Server* server = running.load()) {
    server->stop();
  }
}

int run_serve(const blogin::cli::Command& command) {
  const std::filesystem::path content = command.content;

  auto config = configuration(root_of(content));

  if (!config) {
    return fail(config.error());
  }

  blogin::ServeOptions options;
  options.build = blogin::BuildOptions::around(content, blogin::reload::preview_config(*config));
  options.port = command.port;
  options.log = log_for(command);

  const std::filesystem::path serving = options.build.output;

  blogin::Server server(std::move(options));

  if (auto listening = server.listen(); !listening) {
    return fail(listening.error().message);
  }

  running.store(&server);

  std::signal(SIGINT, handle_interrupt);
  std::signal(SIGTERM, handle_interrupt);

  std::cout << "serving " << serving.string() << " on http://127.0.0.1:" << server.port()
            << "\n";

  auto report = server.run();

  running.store(nullptr);

  if (!report) {
    return fail(report.error().message);
  }

  std::cout << "\nstopped after " << report->requests << " requests and " << report->rebuilds
            << " builds\n";

  return 0;
}

}  // namespace

// A build that runs out of memory, or a filesystem that answers with an
// exception, ends with a message and a status rather than with whatever the
// runtime prints when one escapes.
int main(int argc, char** argv) {
  try {
    const std::vector<std::string> arguments(argv + (argc > 0 ? 1 : 0), argv + argc);

    auto command = blogin::cli::parse(arguments);

    if (!command) {
      std::cerr << "blogin: " << command.error().message << "\n\n" << blogin::cli::usage();

      return 2;
    }

    switch (command->verb) {
      case blogin::cli::Verb::version:
        std::cout << "blogin " << blogin::cli::version() << "\n";

        return 0;
      case blogin::cli::Verb::build:
        return run_build(*command);
      case blogin::cli::Verb::init:
        return run_init(*command);
      case blogin::cli::Verb::new_post:
        return run_new(*command);
      case blogin::cli::Verb::clean:
        return run_clean(*command);
      case blogin::cli::Verb::serve:
        return run_serve(*command);
    }

    return 2;
  } catch (const std::exception& failure) {
    std::cerr << "blogin: " << failure.what() << "\n";

    return 1;
  }
}
