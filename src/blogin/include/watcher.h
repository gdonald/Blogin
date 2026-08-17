#pragma once

#include <atomic>
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "json.h"

namespace blogin {

// Watches a set of directories and says when something under them changed.
//
// One interface over FSEvents and inotify. Neither reports what changed
// reliably enough to act on, so this reports that something happened and leaves
// the build to work out what.
//
// FSEvents rather than kqueue on macOS: kqueue on a directory misses an editor
// writing a file in place, and kqueue per file costs a descriptor each.
class Watcher {
public:
  // `excluded` is what to ignore inside those directories, which in practice is
  // the output tree: a build writing its own output must not wake the watcher
  // that started it.
  //
  // Directories created after the watcher starts are picked up too, since
  // a new section appears without a restart.
  static std::expected<std::unique_ptr<Watcher>, ParseError> watch(
    const std::vector<std::filesystem::path>& roots,
    const std::vector<std::filesystem::path>& excluded = {});

  virtual ~Watcher() = default;

  // Blocks until something changes or the timeout runs out. True when something
  // changed.
  virtual bool wait(std::chrono::milliseconds timeout) = 0;

  // Makes a blocked wait return. Safe from another thread, so a
  // signal handler stops the server.
  virtual void stop() = 0;

  // How many directories are being watched, the number that runs into the
  // per-user limits.
  virtual std::size_t watched() const = 0;

protected:
  Watcher() = default;
};

// Whether a path a recursive watcher delivered is an edit worth rebuilding for.
//
// A recursive watcher reports paths the watch list never asked for. Two kinds
// are not edits: anything the build wrote, and anything under a dot directory,
// so `.git/index` does not wake a rebuild every few seconds.
//
// `roots` and `excluded` are canonical. Only the part of `path` below a root is
// examined for dot directories, since the root itself may sit inside one.
bool ignored_change(std::string_view path, const std::vector<std::string>& roots,
                    const std::vector<std::string>& excluded);

// Every directory under `root`, itself included, skipping dot directories and
// the output tree. This is what a watcher is pointed at, and what the count of
// watches is decided by.
std::vector<std::filesystem::path> watchable_directories(const std::filesystem::path& root,
                                                         const std::filesystem::path& output);

}  // namespace blogin
