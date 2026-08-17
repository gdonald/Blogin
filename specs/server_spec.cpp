#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "reload.h"
#include "server.h"
#include "watcher.h"
#include "support/spec.h"
#include "websocket.h"

using spec::expect;

namespace {

void write(const std::filesystem::path& path, std::string_view body) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

std::filesystem::path preview_site() {
  const std::filesystem::path root = spec::scratch_directory("server");

  write(root / "blogin.json",
        R"({"title":"Preview","base-url":"https://example.com","search":false,"minify":false,)"
        R"("fingerprint":false,"home-section":"posts"})");

  write(root / "layouts" / "base.haml", "!!! 5\n%html\n  %body\n    != yield\n");
  write(root / "layouts" / "show.haml", "%article\n  %h1= title\n  != body\n");
  write(root / "layouts" / "index.haml", "%section\n  %h1= heading\n");
  write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nA post.\n");

  // The home section, so the site has a root index to serve.
  write(root / "content" / "posts" / "first.md", "---\ntitle: First\n---\nThe first one.\n");
  write(root / "assets" / "css" / "style.css", "body{color:red}");

  return root;
}

blogin::ServeOptions options_for(const std::filesystem::path& root) {
  const auto config = blogin::Config::load(root / "blogin.json").value();

  blogin::ServeOptions options;
  options.build = blogin::BuildOptions::around(root / "content", config);

  // Zero means the system picks, so examples running side by side never fight
  // over a number.
  options.port = 0;
  options.watch = false;

  // Quiet unless an example is about what the server says, in which case it
  // hands over a stream of its own to read back.
  options.log = blogin::Log(blogin::LogLevel::quiet);

  return options;
}

// A socket to the server under test, with enough of a client to drive a
// request and read a frame back.
class Client {
public:
  explicit Client(int port) : socket_(::socket(AF_INET, SOCK_STREAM, 0)) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    // Writing to a socket the server has closed must fail. An unhandled
    // SIGPIPE would kill the whole run.
#ifdef SO_NOSIGPIPE
    int quiet = 1;

    ::setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, &quiet, sizeof(quiet));
#endif

    connected_ = ::connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
  }

  ~Client() {
    if (socket_ >= 0) {
      ::close(socket_);
    }
  }

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  bool connected() const { return connected_; }

  void send(std::string_view text) const {
#ifdef MSG_NOSIGNAL
    ::send(socket_, text.data(), text.size(), MSG_NOSIGNAL);
#else
    ::send(socket_, text.data(), text.size(), 0);
#endif
  }

  // Reads until `marker` turns up, or until the deadline says it is not coming.
  // Reading until the connection goes quiet races a server that is merely busy,
  // which under a sanitizer it often is.
  //
  // The deadline is generous on purpose. It bounds how long a failing example
  // takes without deciding anything, so a slow or loaded machine waits
  // longer and still passes.
  std::string receive_until(std::string_view marker) const {
    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

    while (std::chrono::steady_clock::now() < deadline) {
      out += receive(1);

      if (out.contains(marker)) {
        break;
      }
    }

    return out;
  }

  // True once the server has hung up, which tells a failed read apart from one
  // that has not happened yet.
  bool closed() const { return closed_; }

  // Reads until the connection goes quiet or `wanted` bytes have arrived. The
  // quiet window is what a caller waiting for nothing pays, so it is short, and
  // receive_until is what waits a long time for something.
  std::string receive(std::size_t wanted = 0) const {
    std::string out;
    char chunk[4096];

    for (int attempt = 0; attempt < 25; ++attempt) {
      const ssize_t received = ::recv(socket_, chunk, sizeof(chunk), MSG_DONTWAIT);

      if (received > 0) {
        out.append(chunk, static_cast<std::size_t>(received));

        if (wanted > 0 && out.size() >= wanted) {
          break;
        }

        continue;
      }

      if (received == 0) {
        closed_ = true;

        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    return out;
  }

private:
  int socket_ = -1;
  bool connected_ = false;
  mutable bool closed_ = false;
};

// A watcher that plays a scripted sequence and touches no filesystem.
//
// Coalescing is a decision about a sequence of change notifications, not about
// how fast a machine can write five files. Driving it from a script asserts
// what the loop does on any machine, where writing to disk and hoping the
// writes land inside the debounce window asserts how loaded the machine is.
class ScriptedWatcher : public blogin::Watcher {
public:
  explicit ScriptedWatcher(std::vector<bool> changes) : changes_(std::move(changes)) {}

  // Hands back the next scripted change. Once the script runs out it blocks
  // like a real watcher over a quiet tree, until stop() wakes it.
  bool wait(std::chrono::milliseconds timeout) override {
    std::unique_lock<std::mutex> lock(mutex_);

    if (next_ < changes_.size()) {
      return changes_[next_++];
    }

    quiet_.wait_for(lock, timeout, [this] { return stopping_; });

    return false;
  }

  void stop() override {
    {
      const std::scoped_lock lock(mutex_);
      stopping_ = true;
    }

    quiet_.notify_all();
  }

  std::size_t watched() const override { return 1; }

private:
  std::mutex mutex_;
  std::condition_variable quiet_;
  std::vector<bool> changes_;
  std::size_t next_ = 0;
  bool stopping_ = false;
};

// Points a ServeOptions at a scripted watcher instead of the filesystem.
void watch_script(blogin::ServeOptions& options, std::vector<bool> changes) {
  options.watch = true;
  options.make_watcher = [changes = std::move(changes)](
                           const std::vector<std::filesystem::path>&,
                           const std::vector<std::filesystem::path>&)
    -> std::expected<std::unique_ptr<blogin::Watcher>, blogin::ParseError> {
    return std::make_unique<ScriptedWatcher>(changes);
  };
}

// Runs the server for the length of one example and stops it afterwards,
// however the example ends.
class Running {
public:
  explicit Running(blogin::ServeOptions options) : server_(std::move(options)) {
    listening_ = server_.listen().has_value();

    thread_ = std::thread([this] { report_ = server_.run().value_or(blogin::ServeReport{}); });

    // Binding happens before the thread starts, so connecting would succeed
    // while the first build is still running. Waiting for the server to say it
    // is serving keeps the example independent of how loaded the
    // machine is.
    while (!server_.ready()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  ~Running() {
    server_.stop();

    if (thread_.joinable()) {
      thread_.join();
    }
  }

  Running(const Running&) = delete;
  Running& operator=(const Running&) = delete;

  int port() const { return server_.port(); }

  bool listening() const { return listening_; }

  const blogin::ServeReport& report() const { return report_; }

  // Waits for the server to have done the work, never for a length of
  // time. The deadline bounds how long an example that will fail takes to say
  // so, and is not what is being asserted. A loaded machine only makes the wait
  // longer, and never the example flakier.
  void wait_for(const std::function<bool(const blogin::ServeReport&)>& done) const {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

    while (std::chrono::steady_clock::now() < deadline) {
      if (done(server_.progress())) {
        return;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  void wait_for_rebuilds(std::size_t wanted) const {
    wait_for([wanted](const blogin::ServeReport& report) { return report.rebuilds >= wanted; });
  }

  void stop() {
    server_.stop();

    if (thread_.joinable()) {
      thread_.join();
    }
  }

private:
  blogin::Server server_;
  std::thread thread_;
  blogin::ServeReport report_;
  bool listening_ = false;
};

// Waits for a rebuild to have put something on disk, bounded so a build that
// never comes fails the example instead of hanging the run.
void wait_for_file(const std::filesystem::path& path, std::string_view wanted) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

  while (std::chrono::steady_clock::now() < deadline) {
    std::ifstream input(path, std::ios::binary);
    const std::string body((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    if (body.contains(wanted)) {
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

// Everything the server sent up to and including one whole frame after the
// headers.
//
// Stopping after a fixed number of bytes races the frame's arrival: a frame
// split across two packets leaves a payload that is there but not all there,
// which reads as an empty one.
std::string upgrade_answer(const Client& client) {
  std::string answer;

  // Patient on purpose, and bounded by the clock, not by a number of
  // tries. Eight examples running at once, each with its own server and its own
  // build, means a request can wait a while for a thread. This is a limit on how
  // long a broken server is given before the example gives up, not an assertion
  // about how long a working one takes.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

  while (std::chrono::steady_clock::now() < deadline) {
    answer += client.receive(1);

    const auto headers = answer.find("\r\n\r\n");

    if (headers == std::string::npos || answer.size() < headers + 6) {
      continue;
    }

    const auto length =
      static_cast<std::size_t>(static_cast<unsigned char>(answer[headers + 5]) & 0x7f);

    if (answer.size() >= headers + 4 + 2 + length) {
      break;
    }
  }

  return answer;
}

// The payload of a frame the server sent. Server frames are never masked, so
// the decoder built for what a client sends refuses them by design.
std::string server_frame_payload(std::string_view frame) {
  if (frame.size() < 2) {
    return {};
  }

  const auto length = static_cast<std::size_t>(static_cast<unsigned char>(frame[1]) & 0x7f);

  if (length >= 126 || frame.size() < 2 + length) {
    return {};
  }

  return std::string(frame.substr(2, length));
}

// The `"version":N` a reload message or an injected script carries. Both are
// written by the same JSON emitter, so both spell it the same way.
std::int64_t version_in(std::string_view text) {
  const auto start = text.find("\"version\":");

  if (start == std::string_view::npos) {
    return -1;
  }

  const auto digits = start + std::strlen("\"version\":");
  const auto end = text.find_first_not_of("0123456789", digits);

  return std::stoll(std::string(text.substr(digits, end == std::string_view::npos ? end : end - digits)));
}

std::string get(int port, std::string_view path) {
  Client client(port);

  if (!client.connected()) {
    return {};
  }

  client.send(std::string("GET ") + std::string(path) +
              " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

  return client.receive();
}

}  // namespace

SPEC {
  spec::describe("the preview server", [] {
    spec::context("serving", [] {
      spec::it("builds the site before answering anything", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/hello/")).to_contain("<h1>Hello</h1>");
      });

      spec::it("answers with a status line", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/")).to_contain("HTTP/1.1 200 OK");
      });

      spec::it("serves the index for the root", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/")).to_contain("<html>");
      });

      spec::it("resolves an extensionless url", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/hello")).to_contain("<h1>Hello</h1>");
      });

      spec::it("names the content type of a stylesheet", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/assets/css/style.css")).to_contain("Content-Type: text/css");
      });

      spec::it("answers 404 for a url that names nothing", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/nowhere")).to_contain("404 Not Found");
      });

      spec::it("refuses a method it does not serve", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        client.send("POST / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

        expect(client.receive()).to_contain("405 Method Not Allowed");
      });

      // Two requests down one connection, the way a browser loads a page and
      // its stylesheet.
      spec::it("answers a second request on the same connection", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        client.send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        client.receive(1);
        client.send("GET /assets/css/style.css HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

        expect(client.receive_until("text/css")).to_contain("text/css");
      });

      // Two requests can arrive in one read, either because the client sent
      // them together or because the network delivered them together. Waiting
      // for another read before looking at what is already in hand leaves the
      // second one unanswered while the client waits for it.
      spec::it("answers both requests when they arrive in one read", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        client.send("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
                    "GET /assets/css/style.css HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

        expect(client.receive_until("text/css")).to_contain("text/css");
      });

      // A HEAD promises the length a GET would have carried, so a client can
      // decide whether to ask for it.
      spec::it("answers a head request with the length and no body", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        client.send("HEAD / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

        const std::string answer = client.receive();
        const auto headers = answer.find("\r\n\r\n");

        spec::aggregate_failures([&] {
          expect(answer).not_to_contain("Content-Length: 0\r\n");
          expect(answer.size() - headers - 4).to_eq(std::size_t{0});
        });
      });

      spec::it("refuses a request it cannot read", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        // Syntactically a request line and then something that is not a header.
        client.send("GET / HTTP/1.1\r\nrubbish\r\n\r\n");

        expect(client.receive()).to_contain("400 Bad Request");
      });

      spec::it("counts the requests it answered", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        get(running.port(), "/");
        get(running.port(), "/hello");

        running.stop();

        expect(running.report().requests).to_eq(std::size_t{2});
      });
    });

    spec::context("the reload client", [] {
      spec::it("puts the reload script into a page", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/")).to_contain("__blogin-reload");
      });

      spec::it("puts it before the closing body tag", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        const std::string page = get(running.port(), "/");

        expect(page.find("__blogin-reload") < page.find("</body>")).to_be_true();
      });

      // A stylesheet with a script tag pasted into it would not be a stylesheet.
      spec::it("leaves a stylesheet alone", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/assets/css/style.css")).not_to_contain("__blogin-reload");
      });

      spec::it("puts it into the not-found page too", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/nowhere")).to_contain("__blogin-reload");
      });
    });

    spec::context("the reload socket", [] {
      spec::it("accepts the upgrade", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        client.send("GET /__blogin-reload HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                    "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n\r\n");

        const std::string answer = upgrade_answer(client);

        running.stop();

        spec::aggregate_failures([&] {
          expect(client.connected()).to_be_true();
          expect(running.report().accepted).to_eq(std::size_t{1});
          expect(running.report().requests).to_eq(std::size_t{1});
          expect(client.closed()).to_be_false();
          expect(answer).to_contain("101 Switching Protocols");
        });
      });

      spec::it("answers with the accept key for the offered key", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        client.send("GET /__blogin-reload HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                    "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n\r\n");

        const std::string answer = upgrade_answer(client);

        spec::aggregate_failures([&] {
          expect(client.connected()).to_be_true();
          expect(answer).to_contain("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
        });
      });

      // A page that reloaded while the server was rebuilding has to be able to
      // tell that it is behind, which the hello carries.
      spec::it("greets a page with the version and the session", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        Client client(running.port());
        client.send("GET /__blogin-reload HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                    "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n\r\n");

        const std::string answer = upgrade_answer(client);
        const auto headers = answer.find("\r\n\r\n");
        const std::string payload =
          headers == std::string::npos
            ? std::string{}
            : server_frame_payload(std::string_view(answer).substr(headers + 4));

        spec::aggregate_failures([&] {
          expect(payload).to_contain(R"("type":"hello")");
          expect(payload).to_contain("\"session\"");
        });
      });

      // Several pages open at once is ordinary: a couple of browser tabs, or one
      // tab and one reload after an edit. Every one of them has to be answered.
      spec::it("upgrades several connections at once", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        std::vector<std::unique_ptr<Client>> clients;

        for (int index = 0; index < 8; ++index) {
          clients.push_back(std::make_unique<Client>(running.port()));

          clients.back()->send(
            "GET /__blogin-reload HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n");
        }

        std::size_t upgraded = 0;

        for (const auto& client : clients) {
          if (upgrade_answer(*client).contains("101")) {
            ++upgraded;
          }
        }

        expect(upgraded).to_eq(std::size_t{8});
      });

      spec::it("does not upgrade an ordinary request to that path", [] {
        const std::filesystem::path root = preview_site();
        Running running(options_for(root));

        expect(get(running.port(), "/__blogin-reload")).to_contain("404");
      });
    });

    spec::context("binding the port", [] {
      spec::it("reports a port that is already taken", [] {
        const std::filesystem::path root = preview_site();

        Running first(options_for(root));

        blogin::ServeOptions second = options_for(root);
        second.port = first.port();

        blogin::Server other(second);

        expect(other.listen().error().message).to_contain("already in use");
      });

      spec::it("says which port to change", [] {
        const std::filesystem::path root = preview_site();

        Running first(options_for(root));

        blogin::ServeOptions second = options_for(root);
        second.port = first.port();

        blogin::Server other(second);

        expect(other.listen().error().message).to_contain("--port");
      });
    });
  });

  // An edit reaches the page. Asserted as work done, never as elapsed
  // time, so it means the same on a slow machine.
  spec::describe("the round trip from save to reload", [] {
    spec::it("rebuilds once for one edit", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nEdited.\n");

      running.wait_for_rebuilds(2);
      running.stop();

      // The build at startup, and the one the edit caused.
      expect(running.report().rebuilds).to_eq(std::size_t{2});
    });

    spec::it("pushes once for one edit", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nEdited.\n");

      running.wait_for([](const blogin::ServeReport& report) { return report.pushes >= 1; });
      running.stop();

      expect(running.report().pushes).to_eq(std::size_t{1});
    });

    // A save touches more than one file, and an editor writes each in several
    // goes. All of it has to collapse into one build.
    //
    // Five changes then quiet, played from a script. The build at startup is
    // the first of the two, and the burst is the second.
    spec::it("rebuilds once for a burst of edits", [] {
      blogin::ServeOptions options = options_for(preview_site());
      watch_script(options, {true, true, true, true, true, false});

      Running running(options);

      running.wait_for_rebuilds(2);
      running.stop();

      expect(running.report().rebuilds).to_eq(std::size_t{2});
    });

    // Quiet in the middle of a burst ends it. Two runs of changes with a gap
    // between them are two builds, not one, whatever the wall clock says.
    spec::it("rebuilds again for a second burst", [] {
      blogin::ServeOptions options = options_for(preview_site());
      watch_script(options, {true, true, false, true, true, false});

      Running running(options);

      running.wait_for_rebuilds(3);
      running.stop();

      expect(running.report().rebuilds).to_eq(std::size_t{3});
    });

    // A watcher that reports nothing produces no rebuild and nothing to push.
    // Asserted from an empty script, not from a sleep long enough to
    // believe nothing is coming, which a loaded machine makes a lie.
    spec::it("says nothing when nothing changed", [] {
      blogin::ServeOptions options = options_for(preview_site());
      watch_script(options, {});

      Running running(options);

      running.stop();

      expect(running.report().pushes).to_eq(std::size_t{0});
    });

    spec::it("serves the edited page after the rebuild", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nThe new words.\n");

      // A rebuild having happened is not the same as this edit having been
      // built: the watcher may report the fixture's own creation first. Waiting
      // for the page on disk waits for the right one, and what is asserted is
      // still that the server hands it over.
      wait_for_file(options.build.output / "hello" / "index.html", "The new words.");

      expect(get(running.port(), "/hello")).to_contain("The new words.");
    });

    // The client reloads when the version in the page it is running in differs
    // from the version the socket greets it with, so a page served
    // before a build catches up. So every build has to change what a page is
    // served with: while it does not, each reload produces another page that
    // disagrees, and the tab reloads for as long as it is open.
    spec::it("serves a page carrying the version the build left behind", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      const std::int64_t before = version_in(get(running.port(), "/hello"));

      write(root / "content" / "hello.md", "---\ntitle: Hello\n---\nEdited once.\n");

      // The version moves when the change is pushed, which is after the page it
      // describes has been written, so waiting for the push waits for both.
      running.wait_for([](const blogin::ServeReport& report) { return report.pushes >= 1; });

      expect(version_in(get(running.port(), "/hello"))).to_be_greater_than(before);
    });
  });

  // A failed rebuild reaches the open page and not only the terminal, so
  // the error is seen where the change was made.
  spec::describe("a build that fails", [] {
    spec::it("counts the failure", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      write(root / "layouts" / "show.haml", "%article\n  != no-such-name\n");

      running.wait_for([](const blogin::ServeReport& report) { return report.failures >= 1; });

      running.stop();

      expect(running.report().failures).to_eq(std::size_t{1});
    });

    spec::it("tells a page that connects afterwards", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      write(root / "layouts" / "show.haml", "%article\n  != no-such-name\n");

      running.wait_for([](const blogin::ServeReport& report) { return report.failures >= 1; });

      Client client(running.port());
      client.send("GET /__blogin-reload HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                  "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                  "Sec-WebSocket-Version: 13\r\n\r\n");

      const std::string answer = upgrade_answer(client);

      expect(answer).to_contain(R"("type":"failure")");
    });

    spec::it("carries what went wrong", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      write(root / "layouts" / "show.haml", "%article\n  != no-such-name\n");

      running.wait_for([](const blogin::ServeReport& report) { return report.failures >= 1; });

      Client client(running.port());
      client.send("GET /__blogin-reload HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                  "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                  "Sec-WebSocket-Version: 13\r\n\r\n");

      expect(upgrade_answer(client)).to_contain("no such name");
    });

    // Clearing the overlay is what tells you the fix worked.
    spec::it("stops saying so once the build is good again", [] {
      const std::filesystem::path root = preview_site();

      blogin::ServeOptions options = options_for(root);
      options.watch = true;

      Running running(options);

      write(root / "layouts" / "show.haml", "%article\n  != no-such-name\n");
      running.wait_for([](const blogin::ServeReport& report) { return report.failures >= 1; });

      write(root / "layouts" / "show.haml", "%article\n  %h1= title\n  != body\n");
      running.wait_for_rebuilds(3);

      Client client(running.port());
      client.send("GET /__blogin-reload HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                  "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                  "Sec-WebSocket-Version: 13\r\n\r\n");

      expect(upgrade_answer(client)).not_to_contain("failure");
    });
  });

  // Watching an empty path watches nothing, and a preview that watches nothing
  // serves the site correctly and never rebuilds it.
  spec::describe("finding the site root", [] {
    spec::it("takes the parent of the content directory", [] {
      expect(blogin::site_root("/sites/example/content").string()).to_eq("/sites/example");
    });

    spec::it("takes the working directory when content was named on its own", [] {
      expect(blogin::site_root("content").string()).to_eq(".");
    });
  });

  spec::describe("coalescing edits", [] {
    // A fixed window was free when a rebuild took seventeen seconds. Once it
    // takes eight milliseconds, waiting 200ms to find out is the whole cost.
    spec::it("waits about as long as the last build took", [] {
      expect(blogin::debounce_for(std::chrono::milliseconds(50), std::chrono::milliseconds(20),
                                  std::chrono::milliseconds(200))
               .count())
        .to_eq(std::int64_t{50});
    });

    spec::it("never waits less than the shortest window", [] {
      expect(blogin::debounce_for(std::chrono::milliseconds(1), std::chrono::milliseconds(20),
                                  std::chrono::milliseconds(200))
               .count())
        .to_eq(std::int64_t{20});
    });

    spec::it("never waits longer than the longest window", [] {
      expect(blogin::debounce_for(std::chrono::milliseconds(5000), std::chrono::milliseconds(20),
                                  std::chrono::milliseconds(200))
               .count())
        .to_eq(std::int64_t{200});
    });
  });

  // A preview server that says nothing gives no way to tell a request that was
  // never made from one that was answered wrongly.
  spec::describe("what the preview server reports", [] {
    // Written to by the connection threads, so it outlives the server rather
    // than the example's stack frame.
    auto written = spec::let([] { return std::make_shared<std::ostringstream>(); });

    const auto talking = [](const std::filesystem::path& root, blogin::LogLevel level,
                            std::ostringstream& out) {
      blogin::ServeOptions options = options_for(root);
      options.log = blogin::Log(level, out, out);

      return options;
    };

    spec::it("reports each request when asked for everything", [=] {
      const std::filesystem::path root = preview_site();
      Running running(talking(root, blogin::LogLevel::verbose, *written()));

      get(running.port(), "/hello");
      running.stop();

      expect(written()->str()).to_contain("GET /hello 200");
    });

    spec::it("reports how long a request took", [=] {
      const std::filesystem::path root = preview_site();
      Running running(talking(root, blogin::LogLevel::verbose, *written()));

      get(running.port(), "/hello");
      running.stop();

      expect(written()->str()).to_contain("ms");
    });

    spec::it("reports a url that named nothing", [=] {
      const std::filesystem::path root = preview_site();
      Running running(talking(root, blogin::LogLevel::verbose, *written()));

      get(running.port(), "/nowhere");
      running.stop();

      expect(written()->str()).to_contain("GET /nowhere 404");
    });

    spec::it("says nothing about a request at the normal level", [=] {
      const std::filesystem::path root = preview_site();
      Running running(talking(root, blogin::LogLevel::normal, *written()));

      get(running.port(), "/hello");
      running.stop();

      expect(written()->str()).not_to_contain("GET /hello");
    });

    spec::it("reports what each build produced", [=] {
      const std::filesystem::path root = preview_site();
      Running running(talking(root, blogin::LogLevel::normal, *written()));

      running.stop();

      expect(written()->str()).to_contain("built 2 pages");
    });

    // The overlay is not the only place a failure belongs: the terminal is
    // where somebody looks when the page is not open.
    spec::it("reports a build that failed", [=] {
      const std::filesystem::path root = preview_site();

      write(root / "layouts" / "broken.haml", "%a{href: 'x'\n");

      Running running(talking(root, blogin::LogLevel::normal, *written()));

      running.stop();

      expect(written()->str()).to_contain("build failed");
    });
  });
}
