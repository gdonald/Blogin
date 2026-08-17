#include "server.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <format>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "files.h"
#include "http.h"
#include "reload.h"
#include "watcher.h"
#include "websocket.h"

namespace blogin {
namespace {

// Long enough that a browser holding a keep-alive connection open is not
// dropped mid-read, short enough that a dead one is not held forever.
constexpr std::chrono::milliseconds poll_interval{100};

// Writing to a socket the browser has closed must fail, not kill the process.
// Linux says so per write, and macOS says so per socket.
#ifdef MSG_NOSIGNAL
constexpr int send_flags = MSG_NOSIGNAL;
#else
constexpr int send_flags = 0;
#endif

void quiet_on_close(int socket) {
#ifdef SO_NOSIGPIPE
  int on = 1;

  ::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
  (void)socket;
#endif
}

bool send_all(int socket, std::string_view data) {
  std::size_t sent = 0;

  while (sent < data.size()) {
    const ssize_t written = ::send(socket, data.data() + sent, data.size() - sent, send_flags);

    if (written <= 0) {
      // A page that navigated away closes mid-write, which is ordinary.
      if (written < 0 && errno == EINTR) {
        continue;
      }

      return false;
    }

    sent += static_cast<std::size_t>(written);
  }

  return true;
}

}  // namespace

std::filesystem::path site_root(const std::filesystem::path& content) {
  const std::filesystem::path parent = content.parent_path();

  return parent.empty() ? std::filesystem::path(".") : parent;
}

std::chrono::milliseconds debounce_for(std::chrono::milliseconds last_build,
                                       std::chrono::milliseconds shortest,
                                       std::chrono::milliseconds longest) {
  return std::clamp(last_build, shortest, longest);
}

// Everything the request threads and the watch thread share.
struct Server::State {
  explicit State(ServeOptions given) : options(std::move(given)) {}

  ServeOptions options;

  int listener = -1;
  int bound_port = 0;

  std::atomic<bool> stopping{false};
  std::atomic<bool> serving{false};

  // Set once the watcher has registered its directories, or once it is known
  // there will not be one. A save made before that point reaches nobody, so a
  // server is not ready until it can notice one.
  std::atomic<bool> watching{false};

  reload::Channel channel;

  // The reload client as it is injected into a page, carrying the version that
  // was current when the page was served.
  //
  // A page reloads itself when the socket greets it with a version other than
  // the one it carries. Injecting one script for the life of the server would
  // stamp every page with the version from before the first build, so every
  // greeting disagrees and the tab reloads for as long as it is open. It is
  // rebuilt whenever the version moves.
  std::shared_ptr<const std::string> client_script() {
    const std::int64_t version = channel.version();

    const std::scoped_lock guard(script_mutex);

    if (script == nullptr || script_version != version) {
      script = std::make_shared<const std::string>(
        reload::client_script(version, channel.session()));
      script_version = version;
    }

    return script;
  }

  std::mutex script_mutex;
  std::shared_ptr<const std::string> script;
  std::int64_t script_version = -1;

  // Connected preview pages, by socket. Guarded because the watch thread writes
  // to them while request threads add and remove them.
  std::mutex sockets_mutex;
  std::vector<int> sockets;

  // Every connection being served, and the thread serving it. A connection
  // thread reads this server's state, so the server cannot go away while one is
  // still running: they are joined before run returns, and their sockets are
  // shut down first so a thread blocked in recv comes back.
  //
  // A finished one is joined on the way past, so
  // a long preview session does not accumulate a thread per page it served.
  // The socket is closed here, not by the thread that served it. Closed
  // while still listed, its number can be reused by any socket in the process,
  // and teardown would then shut down an unrelated connection.
  struct Connection {
    std::thread thread;
    int socket = -1;
    std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);

    // An upgraded socket belongs to the push list, which closes it.
    std::shared_ptr<std::atomic<bool>> upgraded = std::make_shared<std::atomic<bool>>(false);
  };

  std::mutex connections_mutex;
  std::vector<Connection> connections;

  static void release(Connection& connection) {
    if (connection.thread.joinable()) {
      connection.thread.join();
    }

    if (!connection.upgraded->load()) {
      ::close(connection.socket);
    }
  }

  void reap() {
    const std::scoped_lock guard(connections_mutex);

    for (auto entry = connections.begin(); entry != connections.end();) {
      if (entry->done->load()) {
        release(*entry);

        entry = connections.erase(entry);
      } else {
        ++entry;
      }
    }
  }

  std::mutex report_mutex;
  ServeReport report;

  // The last failure, so a page connecting after a bad build is told about it.
  std::mutex failure_mutex;
  std::string standing_failure;

  std::chrono::milliseconds last_build{0};

  void count_request() {
    const std::scoped_lock guard(report_mutex);

    ++report.requests;
  }

  void push(const std::string& message) {
    const std::string frame = websocket::encode_frame(websocket::Opcode::text, message);

    std::vector<int> living;

    const std::scoped_lock guard(sockets_mutex);

    for (const int socket : sockets) {
      if (send_all(socket, frame)) {
        living.push_back(socket);
      } else {
        ::close(socket);
      }
    }

    sockets = std::move(living);
  }
};

Server::Server(ServeOptions options) : state_(std::make_unique<State>(std::move(options))) {}

Server::~Server() {
  if (state_->listener >= 0) {
    ::close(state_->listener);
  }
}

int Server::port() const {
  return state_->bound_port;
}

void Server::stop() {
  state_->stopping.store(true);
}

bool Server::ready() const {
  return state_->serving.load() && state_->watching.load();
}

ServeReport Server::progress() const {
  const std::scoped_lock guard(state_->report_mutex);

  return state_->report;
}

std::expected<void, ParseError> Server::listen() {
  const int listener = ::socket(AF_INET, SOCK_STREAM, 0);

  if (listener < 0) {
    return std::unexpected(ParseError{"cannot open a socket", 1, 1});
  }

  int reuse = 1;
  ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(state_->options.port));
  ::inet_pton(AF_INET, state_->options.host.c_str(), &address.sin_addr);

  if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(listener);

    return std::unexpected(ParseError{
      std::format("port {} is already in use. Pass --port to choose another", state_->options.port), 1,
      1});
  }

  if (::listen(listener, 64) != 0) {
    ::close(listener);

    return std::unexpected(ParseError{"cannot listen on the socket", 1, 1});
  }

  // Port zero asks the system to choose, so concurrent specs do not fight over
  // a number.
  sockaddr_in bound{};
  socklen_t length = sizeof(bound);

  if (::getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &length) == 0) {
    state_->bound_port = ntohs(bound.sin_port);
  } else {
    state_->bound_port = state_->options.port;
  }

  state_->listener = listener;

  return {};
}

std::expected<ServeReport, ParseError> Server::run() {
  if (state_->listener < 0) {
    if (auto listening = listen(); !listening) {
      return std::unexpected(listening.error());
    }
  }

  State& state = *state_;

  // One build before the first request, so a page has something to show.
  // `announce` is false for the build at startup: no page is connected yet, and
  // counting a push to nobody would make the round trip look like two.
  const auto build_once = [&state](bool announce) -> std::vector<std::string> {
    const auto started = std::chrono::steady_clock::now();

    // The whole site, so a preview of a multi-language site serves every
    // language.
    auto report = build_site(state.options.build);

    state.last_build = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);

    {
      const std::scoped_lock guard(state.report_mutex);

      ++state.report.rebuilds;
    }

    if (!report) {
      const ParseError& failure = report.error();

      // The overlay is not the only place a failure belongs: the terminal is
      // where somebody looks when the page is not open.
      state.options.log.error(std::format("build failed: {}", failure.message));

      {
        const std::scoped_lock guard(state.failure_mutex);

        // A build error names its own file inside the message, which the
        // overlay shows.
        state.standing_failure = state.channel.failure(failure.message, "", failure.line);
      }

      {
        const std::scoped_lock guard(state.report_mutex);

        ++state.report.failures;
      }

      std::string message;

      {
        const std::scoped_lock guard(state.failure_mutex);

        message = state.standing_failure;
      }

      if (announce) {
        state.push(message);

        const std::scoped_lock guard(state.report_mutex);

        ++state.report.pushes;
      }

      return {};
    }

    // A build that failed and then succeeded has to say so, or the overlay
    // stays up over a page that is now correct.
    bool had_failure = false;

    {
      const std::scoped_lock guard(state.failure_mutex);

      had_failure = !state.standing_failure.empty();
      state.standing_failure.clear();
    }

    std::vector<std::string> paths =
      reload::changed_paths(report->changed, state.options.build.output);

    state.options.log.info(std::format("built {} pages ({} rendered), {} written in {}ms",
                                       report->pages, report->rendered, report->written,
                                       state.last_build.count()));

    // Only for a rebuild. The build at startup writes the whole site, and
    // listing every page of it says nothing.
    if (announce) {
      for (const std::string& path : paths) {
        state.options.log.verbose(std::format("  changed {}", path));
      }
    }

    if (announce && (!paths.empty() || had_failure)) {
      state.push(state.channel.change(paths));

      const std::scoped_lock guard(state.report_mutex);

      ++state.report.pushes;
    }

    return paths;
  };

  build_once(false);

  // Watching runs on its own thread so the request path is never waiting on an
  // editor, and vice versa.
  std::thread watching;

  if (state.options.watch) {
    watching = std::thread([&state, &build_once] {
      const std::filesystem::path root = site_root(state.options.build.content);

      auto watcher = state.options.make_watcher(
        watchable_directories(root, state.options.build.output), {state.options.build.output});

      state.watching.store(true);

      if (!watcher) {
        state.options.log.warn(std::format("not watching for changes: {}", watcher.error().message));

        return;
      }

      state.options.log.verbose(
        std::format("watching {} directories for changes", (*watcher)->watched()));

      while (!state.stopping.load()) {
        if (!(*watcher)->wait(poll_interval)) {
          continue;
        }

        // An editor writes a file in several goes, and a save touches more than
        // one file. Coalescing over about as long as the last build took
        // collapses that into one rebuild without spending longer deciding than
        // building.
        const std::chrono::milliseconds window = debounce_for(
          state.last_build, state.options.minimum_debounce, state.options.maximum_debounce);

        while ((*watcher)->wait(window)) {
          if (state.stopping.load()) {
            return;
          }
        }

        if (state.stopping.load()) {
          return;
        }

        build_once(true);
      }

      (*watcher)->stop();
    });
  }

  if (!state.options.watch) {
    state.watching.store(true);
  }

  state.serving.store(true);

  while (!state.stopping.load()) {
    pollfd waiting{};
    waiting.fd = state.listener;
    waiting.events = POLLIN;

    if (::poll(&waiting, 1, static_cast<int>(poll_interval.count())) <= 0) {
      continue;
    }

    const int connection = ::accept(state.listener, nullptr, nullptr);

    if (connection < 0) {
      continue;
    }

    quiet_on_close(connection);

    {
      const std::scoped_lock guard(state.report_mutex);

      ++state.report.accepted;
    }

    // Each connection is served on its own thread. A preview holds one socket
    // open for its reload channel, so serving them in turn would mean serving
    // exactly one page.
    state.reap();

    auto done = std::make_shared<std::atomic<bool>>(false);
    auto was_upgraded = std::make_shared<std::atomic<bool>>(false);

    std::thread serving([&state, connection, done, was_upgraded] {
      bool upgraded = false;

      // An exception leaving a thread body terminates the process. A connection
      // that runs out of memory, or reads a file that answers with an
      // exception, takes only itself down.
      try {
        std::string buffer;

        while (!state.stopping.load()) {
          const auto parsed = http::parse_request(buffer);

          // What arrived earlier can already hold the next request, either
          // because the client sent both together or because one read
          // delivered both. Reading again before looking at what is in hand
          // leaves that request unanswered while the client waits, and the
          // connection hangs.
          if (parsed.state == http::RequestState::incomplete) {
            char chunk[8192];
            const ssize_t received = ::recv(connection, chunk, sizeof(chunk), 0);

            if (received <= 0) {
              break;
            }

            buffer.append(chunk, static_cast<std::size_t>(received));

            continue;
          }

          if (parsed.state == http::RequestState::malformed) {
            http::Response response;
            response.status = 400;
            response.body = "Bad Request";
            response.keep_alive = false;

            send_all(connection, http::serialize(response));

            break;
          }

          const http::Request& request = parsed.request;
          buffer.erase(0, request.length);

          state.count_request();

          if (request.path == reload::socket_path &&
              websocket::is_upgrade(request.header("connection"), request.header("upgrade"))) {
            http::Response response;
            response.status = 101;
            response.headers.emplace("Upgrade", "websocket");
            response.headers.emplace("Connection", "Upgrade");
            response.headers.emplace("Sec-WebSocket-Accept",
                                     websocket::accept_key(request.header("sec-websocket-key")));

            if (!send_all(connection, http::serialize(response))) {
              break;
            }

            send_all(connection, websocket::encode_frame(websocket::Opcode::text, state.channel.hello()));

            // A page connecting while a build is broken is told so at once, rather
            // than waiting for the next edit to find out.
            std::string standing;

            {
              const std::scoped_lock guard(state.failure_mutex);

              standing = state.standing_failure;
            }

            if (!standing.empty()) {
              send_all(connection, websocket::encode_frame(websocket::Opcode::text, standing));
            }

            {
              const std::scoped_lock guard(state.sockets_mutex);

              state.sockets.push_back(connection);
            }

            upgraded = true;

            state.options.log.verbose(std::format("{} {} 101 reload socket connected",
                                                  request.method, request.target));

            break;
          }

          const auto started = std::chrono::steady_clock::now();

          // What a request cost and what it answered with, which is the first
          // thing anybody wants from a preview server when a page looks wrong.
          const auto report_request = [&state, &request, started](int status, std::size_t bytes) {
            const auto spent = std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - started);

            state.options.log.verbose(std::format("{} {} {} {} bytes {}.{:03}ms", request.method,
                                                  request.target, status, bytes,
                                                  spent.count() / 1000, spent.count() % 1000));
          };

          if (request.method != "GET" && request.method != "HEAD") {
            http::Response response;
            response.status = 405;
            response.body = "Method Not Allowed";

            send_all(connection, http::serialize(response));
            report_request(response.status, response.body.size());

            continue;
          }

          http::Response response;

          // Fetched per page, so a page carries the version current as it is
          // served.
          const std::shared_ptr<const std::string> script = state.client_script();

          if (const auto file = http::resolve_file(request.path, state.options.build.output)) {
            response.content_type = http::content_type_for(*file);
            response.body = files::read_file(*file);

            // Only a page gets the reload client, and only a page needs it.
            if (response.content_type.starts_with("text/html")) {
              response.body = reload::inject(response.body, *script);
            }
          } else {
            response.status = 404;
            response.content_type = "text/html; charset=utf-8";
            response.body = reload::inject("<!doctype html><title>Not Found</title><p>Not Found",
                                           *script);
          }

          response.keep_alive = request.wants_keep_alive();

          if (request.method == "HEAD") {
            const std::size_t length = response.body.size();
            response.body.clear();

            std::string serialized = http::serialize(response);

            const auto placeholder = serialized.find("Content-Length: 0\r\n");

            if (placeholder != std::string::npos) {
              serialized.replace(placeholder, std::strlen("Content-Length: 0\r\n"),
                                 std::format("Content-Length: {}\r\n", length));
            }

            if (!send_all(connection, serialized)) {
              break;
            }

            report_request(response.status, length);
          } else if (!send_all(connection, http::serialize(response))) {
            break;
          } else {
            report_request(response.status, response.body.size());
          }

          if (!response.keep_alive) {
            break;
          }
        }
      } catch (...) {
        // Nothing this connection can still answer with. Shutting the socket
        // down ends the page's request and allocates nothing, which a handler
        // of last resort cannot afford. The descriptor is closed by whoever
        // joins this thread.
        ::shutdown(connection, SHUT_RDWR);
      }

      // The socket stays open until the connection is joined: closing it here
      // would free its number while this connection is still listed.
      was_upgraded->store(upgraded);

      done->store(true);
    });

    {
      const std::scoped_lock guard(state.connections_mutex);

      state.connections.push_back(
        State::Connection{std::move(serving), connection, std::move(done), std::move(was_upgraded)});
    }
  }

  if (watching.joinable()) {
    watching.join();
  }

  // A connection thread may be blocked reading from a browser that has said
  // nothing for a while. Shutting the socket down makes that read return, so
  // the thread can be joined before the state it reads goes away.
  {
    const std::scoped_lock guard(state.connections_mutex);

    for (const State::Connection& connection : state.connections) {
      ::shutdown(connection.socket, SHUT_RDWR);
    }
  }

  {
    const std::scoped_lock guard(state.sockets_mutex);

    for (const int socket : state.sockets) {
      ::shutdown(socket, SHUT_RDWR);
    }
  }

  std::vector<State::Connection> remaining;

  {
    const std::scoped_lock guard(state.connections_mutex);

    remaining = std::move(state.connections);
    state.connections.clear();
  }

  for (State::Connection& connection : remaining) {
    State::release(connection);
  }

  {
    const std::scoped_lock guard(state.sockets_mutex);

    for (const int socket : state.sockets) {
      ::close(socket);
    }

    state.sockets.clear();
  }

  const std::scoped_lock guard(state.report_mutex);

  return state.report;
}

}  // namespace blogin
