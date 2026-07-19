#include "support/spec.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <system_error>
#include <thread>

#include <unistd.h>

namespace spec {
namespace {

struct Group;

struct Example {
  std::string name;
  std::string full_name;
  std::function<void()> body;
  bool pending = false;
  const Group* group = nullptr;
};

struct Group {
  std::string name;
  std::string full_name;
  const Group* parent = nullptr;
  std::vector<std::function<void()>> before_hooks;
  std::vector<std::function<void()>> after_hooks;
  bool serial = false;
};

bool inherits_serial(const Group* group) {
  for (const Group* walk = group; walk != nullptr; walk = walk->parent) {
    if (walk->serial) {
      return true;
    }
  }

  return false;
}

struct Registry {
  std::vector<std::unique_ptr<Group>> groups;
  std::vector<Example> examples;
  std::vector<void (*)()> files;
};

Registry& registry() {
  static Registry instance;

  return instance;
}

// Registration is single threaded and happens before the runner starts.
const Group*& current_group() {
  static const Group* group = nullptr;

  return group;
}

Group* mutable_current_group() {
  return const_cast<Group*>(current_group());  // NOLINT: registration owns the tree
}

std::string join(std::string_view outer, std::string_view inner) {
  if (outer.empty()) {
    return std::string(inner);
  }

  return std::string(outer) + " " + std::string(inner);
}

struct Failure {
  std::string message;
  std::source_location where;
};

// Execution state for the example running on this thread.
struct ExampleState {
  std::vector<Failure> failures;
  std::map<const void*, std::any> lets;
  bool aggregating = false;
};

ExampleState& state() {
  static thread_local ExampleState value;

  return value;
}

struct AbortExample : std::exception {};

std::string relative_path(std::string_view path) {
  const std::filesystem::path full(path);

  return full.filename().string();
}

void open_group(std::string name, const std::function<void()>& body) {
  auto group = std::make_unique<Group>();

  group->name = std::move(name);
  group->parent = current_group();
  group->full_name = join(group->parent != nullptr ? group->parent->full_name : "", group->name);

  Group* raw = group.get();
  registry().groups.push_back(std::move(group));

  const Group* previous = current_group();
  current_group() = raw;

  body();

  current_group() = previous;
}

void collect_hooks(const Group* group, std::vector<const Group*>& chain) {
  if (group == nullptr) {
    return;
  }

  collect_hooks(group->parent, chain);
  chain.push_back(group);
}

struct Options {
  std::string filter;
  unsigned jobs = 1;
  bool list_only = false;
};

Options parse_options(int argc, char** argv) {
  Options options;
  const std::vector<std::string> arguments(argv, argv + argc);

  for (std::size_t index = 1; index < arguments.size(); ++index) {
    const std::string& argument = arguments[index];

    if (argument == "--jobs" && index + 1 < arguments.size()) {
      options.jobs = static_cast<unsigned>(std::stoul(arguments[++index]));
    } else if (argument == "--list") {
      options.list_only = true;
    } else if (argument.starts_with("--")) {
      throw std::runtime_error("unknown option " + argument);
    } else {
      options.filter = argument;
    }
  }

  return options;
}

struct Result {
  std::string full_name;
  std::vector<Failure> failures;
  bool pending = false;
  std::string pending_reason;
  std::string error;
};

// Thrown by spec::pending to leave the example without failing it.
struct PendingExample {
  std::string reason;
};

Result run_example(const Example& example) {
  Result result;
  result.full_name = example.full_name;
  result.pending = example.pending;

  if (example.pending) {
    return result;
  }

  state().failures.clear();
  state().lets.clear();
  state().aggregating = false;

  std::vector<const Group*> chain;
  collect_hooks(example.group, chain);

  try {
    for (const Group* group : chain) {
      for (const auto& hook : group->before_hooks) {
        hook();
      }
    }

    example.body();
    // NOLINTNEXTLINE(bugprone-empty-catch) the failure is already recorded
  } catch (const AbortExample&) {
  } catch (const PendingExample& reason) {
    result.pending = true;
    result.pending_reason = reason.reason;
  } catch (const std::exception& error) {
    result.error = error.what();
  } catch (...) {
    result.error = "unknown exception";
  }

  for (auto & group : std::views::reverse(chain)) {
    for (const auto& hook : group->after_hooks) {
      try {
        hook();
      } catch (...) {
        result.error = "after hook threw";
      }
    }
  }

  result.failures = state().failures;

  state().lets.clear();

  return result;
}

void report(const Result& result, std::string& out) {
  if (result.pending) {
    out += result.pending_reason.empty()
             ? std::format("  pending  {}\n", result.full_name)
             : std::format("  pending  {} ({})\n", result.full_name, result.pending_reason);
    return;
  }

  if (result.failures.empty() && result.error.empty()) {
    out += std::format("  ok       {}\n", result.full_name);
    return;
  }

  out += std::format("  FAILED   {}\n", result.full_name);

  for (const Failure& failure : result.failures) {
    out += std::format("      {}:{}\n", relative_path(failure.where.file_name()), failure.where.line());

    std::string_view message = failure.message;
    std::size_t start = 0;

    while (start <= message.size()) {
      const auto newline = message.find('\n', start);
      const auto piece = newline == std::string_view::npos ? message.substr(start)
                                                           : message.substr(start, newline - start);

      out += std::format("      {}\n", piece);

      if (newline == std::string_view::npos) {
        break;
      }

      start = newline + 1;
    }
  }

  if (!result.error.empty()) {
    out += std::format("      raised: {}\n", result.error);
  }
}

}  // namespace

std::string describe_value(const std::string& value) {
  return "\"" + value + "\"";
}

std::string describe_value(std::string_view value) {
  return "\"" + std::string(value) + "\"";
}

std::string describe_value(const char* value) {
  return value == nullptr ? "null" : "\"" + std::string(value) + "\"";
}

std::string describe_value(bool value) {
  return value ? "true" : "false";
}

std::string describe_value(std::nullptr_t) {
  return "null";
}

void record_failure(std::string message, std::source_location where) {
  state().failures.push_back(Failure{std::move(message), where});

  if (!state().aggregating) {
    throw AbortExample{};
  }
}

void aggregate_failures(const std::function<void()>& body) {
  const bool previous = state().aggregating;
  state().aggregating = true;

  try {
    body();
  } catch (...) {
    state().aggregating = previous;
    throw;
  }

  state().aggregating = previous;

  if (!state().failures.empty() && !previous) {
    throw AbortExample{};
  }
}

std::any* let_slot(const void* key) {
  return &state().lets[key];
}

void describe(std::string name, const std::function<void()>& body) {
  open_group(std::move(name), body);
}

void context(std::string name, const std::function<void()>& body) {
  open_group(std::move(name), body);
}

void serial() {
  mutable_current_group()->serial = true;
}

void before_each(std::function<void()> hook) {
  mutable_current_group()->before_hooks.push_back(std::move(hook));
}

void after_each(std::function<void()> hook) {
  mutable_current_group()->after_hooks.push_back(std::move(hook));
}

void it(const std::string& name, std::function<void()> body) {
  const Group* group = current_group();

  registry().examples.push_back(Example{
    name,
    join(group != nullptr ? group->full_name : "", name),
    std::move(body),
    false,
    group,
  });
}

namespace {

const std::filesystem::path& scratch_root() {
  static const std::filesystem::path root =
    std::filesystem::temp_directory_path() /
    std::format("blogin-specs-{}", static_cast<long long>(::getpid()));

  return root;
}

}  // namespace

std::filesystem::path scratch_directory(std::string_view label) {
  static std::atomic<int> counter{0};

  const std::filesystem::path path =
    scratch_root() / std::format("{}-{}", label, counter.fetch_add(1));

  std::error_code error;
  std::filesystem::remove_all(path, error);
  std::filesystem::create_directories(path.parent_path(), error);

  return path;
}

void pending(std::string_view reason) {
  throw PendingExample{std::string(reason)};
}

void xit(const std::string& name, std::function<void()> body) {
  const Group* group = current_group();

  registry().examples.push_back(Example{
    name,
    join(group != nullptr ? group->full_name : "", name),
    std::move(body),
    true,
    group,
  });
}

FileRegistrar::FileRegistrar(void (*body)()) {
  registry().files.push_back(body);
}

int run(int argc, char** argv) {
  Options options;

  try {
    options = parse_options(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "blogin_specs: " << error.what() << "\n";
    return 2;
  }

  for (void (*file)() : registry().files) {
    file();
  }

  std::vector<const Example*> selected;

  for (const Example& example : registry().examples) {
    if (options.filter.empty() || example.full_name.contains(options.filter)) {
      selected.push_back(&example);
    }
  }

  if (options.list_only) {
    for (const Example* example : selected) {
      std::cout << example->full_name << "\n";
    }

    return 0;
  }

  std::vector<Result> results(selected.size());

  std::vector<std::size_t> concurrent;
  std::vector<std::size_t> exclusive;

  for (std::size_t index = 0; index < selected.size(); ++index) {
    if (inherits_serial(selected[index]->group)) {
      exclusive.push_back(index);
    } else {
      concurrent.push_back(index);
    }
  }

  const unsigned jobs = std::max(1U, options.jobs);

  std::atomic<std::size_t> next_index{0};

  // Each result is written out as it finishes rather than at the end of the
  // run. A suite that hangs or dies partway would otherwise print nothing at
  // all, which is when knowing how far it got matters most.
  std::mutex printing;

  const auto announce = [&printing](const Result& result) {
    std::string line;

    report(result, line);

    const std::scoped_lock guard(printing);

    std::cout << line << std::flush;
  };

  auto worker = [&] {
    for (;;) {
      const std::size_t position = next_index.fetch_add(1);

      if (position >= concurrent.size()) {
        return;
      }

      const std::size_t index = concurrent[position];
      results[index] = run_example(*selected[index]);

      announce(results[index]);
    }
  };

  if (jobs == 1) {
    worker();
  } else {
    std::vector<std::thread> threads;

    threads.reserve(jobs);
    for (unsigned index = 0; index < jobs; ++index) {
      threads.emplace_back(worker);
    }

    for (std::thread& thread : threads) {
      thread.join();
    }
  }

  for (const std::size_t index : exclusive) {
    results[index] = run_example(*selected[index]);

    announce(results[index]);
  }

  std::size_t failed = 0;
  std::size_t pending = 0;

  for (const Result& result : results) {
    if (result.pending) {
      ++pending;
    } else if (!result.failures.empty() || !result.error.empty()) {
      ++failed;
    }
  }

  std::cout << std::format("\n{} examples, {} failures, {} pending\n", results.size(), failed, pending);

  // The run's own directories, and only its own.
  std::error_code error;
  std::filesystem::remove_all(scratch_root(), error);

  return failed == 0 ? 0 : 1;
}

}  // namespace spec
