#pragma once

#include <atomic>
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>

#include "config.h"
#include "json.h"
#include "log.h"
#include "site.h"
#include "watcher.h"

namespace blogin {

struct ServeOptions {
  BuildOptions build;

  // Where the server says what it is doing. A rebuild and a failed build are
  // reported at the normal level, and every request at the verbose one.
  Log log;

  std::string host = "127.0.0.1";
  int port = 3000;

  // Off in a spec, which drives the request path itself without waiting for
  // an editor.
  bool watch = true;

  // The shortest a burst of editor writes is coalesced over. A rebuild that
  // takes single-digit milliseconds should not be preceded by a fixed wait
  // several times its own length.
  std::chrono::milliseconds minimum_debounce{20};

  std::chrono::milliseconds maximum_debounce{200};

  // How the watch thread gets its watcher. A spec replaces this with one that
  // plays a scripted sequence of changes, so what a burst of edits collapses
  // into is asserted without a clock, a filesystem, or a machine fast enough to
  // land five writes inside the debounce window.
  std::function<std::expected<std::unique_ptr<Watcher>, ParseError>(
    const std::vector<std::filesystem::path>&, const std::vector<std::filesystem::path>&)>
    make_watcher = [](const std::vector<std::filesystem::path>& roots,
                      const std::vector<std::filesystem::path>& excluded) {
      return Watcher::watch(roots, excluded);
    };
};

// What one run of the server did, for a spec to assert on. Counts of work, not
// timings.
struct ServeReport {
  // Connections taken off the listener, as against requests read from them. The
  // two differ when a connection is accepted and never answered, which is the
  // shape of a dropped request.
  std::size_t accepted = 0;

  std::size_t requests = 0;
  std::size_t rebuilds = 0;
  std::size_t pushes = 0;
  std::size_t failures = 0;
};

// A preview server. One instance serves one site, and stopping it is safe from
// another thread.
class Server {
public:
  explicit Server(ServeOptions options);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  // Binds the port without serving, so a caller learns the port is taken before
  // it has printed that it is listening.
  std::expected<void, ParseError> listen();

  // Serves until stopped. Builds once first, so the first request has something
  // to answer with.
  std::expected<ServeReport, ParseError> run();

  // Safe from another thread and from a signal handler.
  void stop();

  // True once the first build has finished and the server is accepting. Binding
  // the port happens earlier, so a client can connect before there is anything
  // to answer with, and waiting on the socket alone is a race.
  bool ready() const;

  // The port actually bound, which is the one asked for unless it was zero.
  int port() const;

  // What the server has done so far, while it is still running. A caller
  // waiting for an edit to come back round can wait for the work itself rather
  // than for a length of time it hopes is enough.
  ServeReport progress() const;

private:
  struct State;

  std::unique_ptr<State> state_;
};

// The directory holding the site, given where its content lives.
//
// A relative "content" has no parent as far as the filesystem library is
// concerned, and watching an empty path watches nothing at all, so the answer
// there is the working directory.
std::filesystem::path site_root(const std::filesystem::path& content);

// How long to coalesce filesystem events, given how long the last rebuild took.
//
// A fixed 200ms window was free when a rebuild took seventeen seconds and is
// most of the cost once it takes eight. Waiting about as long as the build
// itself is enough to swallow an editor's burst of writes without spending more
// time deciding than building.
std::chrono::milliseconds debounce_for(std::chrono::milliseconds last_build,
                                       std::chrono::milliseconds shortest,
                                       std::chrono::milliseconds longest);

}  // namespace blogin
