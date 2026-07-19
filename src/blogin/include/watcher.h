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
// kqueue is the obvious macOS answer and the wrong one: watching a directory
// reports entries appearing and disappearing, but an editor writing over a file
// in place changes no directory entry, so most edits go unseen. Watching every
// file instead costs a descriptor each. FSEvents reports the writes themselves,
// recursively, from one stream.
class Watcher {
public:
  // `excluded` is what to ignore inside those directories, which in practice is
  // the output tree: a build writing its own output must not wake the watcher
  // that started it.
  //
  // Directories created after the watcher starts are picked up too, which is why
  // a new section appears without a restart.
  static std::expected<std::unique_ptr<Watcher>, ParseError> watch(
    const std::vector<std::filesystem::path>& roots,
    const std::vector<std::filesystem::path>& excluded = {});

  virtual ~Watcher() = default;

  // Blocks until something changes or the timeout runs out. True when something
  // changed.
  virtual bool wait(std::chrono::milliseconds timeout) = 0;

  // Makes a blocked wait return. Safe from another thread, which is how a
  // signal handler stops the server.
  virtual void stop() = 0;

  // How many directories are being watched, which is what runs into the
  // per-user limits.
  virtual std::size_t watched() const = 0;

protected:
  Watcher() = default;
};

// Whether a path a recursive watcher delivered is an edit worth rebuilding for.
//
// FSEvents watches a whole tree from one stream, so it reports paths the watch
// list never asked for. Two kinds are not edits: anything the build itself
// wrote, and anything under a dot directory. A site is usually a git
// repository, and an editor refreshing `.git/index` would otherwise wake a
// rebuild every few seconds with nothing to do.
//
// `roots` and `excluded` are canonical paths. Only the part of `path` below a
// root is examined for dot directories, since the root itself may sit inside
// one and that says nothing about the file that changed.
bool ignored_change(std::string_view path, const std::vector<std::string>& roots,
                    const std::vector<std::string>& excluded);

// Every directory under `root`, itself included, skipping dot directories and
// the output tree. This is what a watcher is pointed at, and what the count of
// watches is decided by.
std::vector<std::filesystem::path> watchable_directories(const std::filesystem::path& root,
                                                         const std::filesystem::path& output);

}  // namespace blogin
