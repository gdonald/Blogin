#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <utility>
#include <vector>

#include "http.h"
#include "support/spec.h"

using spec::expect;

namespace {

void write(const std::filesystem::path& path, std::string_view body) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(body.data(), static_cast<std::streamsize>(body.size()));
}

// A small tree shaped like published output: an index, a page with an
// extension, and a directory with an index in it.
std::filesystem::path served_tree() {
  const std::filesystem::path root = spec::scratch_directory("http");

  write(root / "index.html", "<html><body>home</body></html>");
  write(root / "about.html", "about");
  write(root / "guide" / "index.html", "guide");
  write(root / "assets" / "css" / "style.css", "body{}");
  write(root.parent_path() / "outside.txt", "not served");

  return root;
}

blogin::http::Request parsed(std::string_view text) {
  return blogin::http::parse_request(text).request;
}

}  // namespace

SPEC {
  spec::describe("reading a request", [] {
    spec::it("reads the method", [] {
      expect(parsed("GET / HTTP/1.1\r\nHost: x\r\n\r\n").method).to_eq("GET");
    });

    spec::it("reads the target", [] {
      expect(parsed("GET /about HTTP/1.1\r\nHost: x\r\n\r\n").target).to_eq("/about");
    });

    spec::it("reads the version", [] {
      expect(parsed("GET / HTTP/1.1\r\nHost: x\r\n\r\n").version).to_eq("HTTP/1.1");
    });

    spec::it("separates the path from the query", [] {
      expect(parsed("GET /a?b=c HTTP/1.1\r\nHost: x\r\n\r\n").path).to_eq("/a");
    });

    // A client writes header names however it likes, so they are looked up
    // without regard to case.
    spec::it("reads a header whatever case it was written in", [] {
      expect(std::string(parsed("GET / HTTP/1.1\r\nHOST: example\r\n\r\n").header("host")))
        .to_eq("example");
    });

    spec::it("reports how many bytes the request took", [] {
      expect(parsed("GET / HTTP/1.1\r\n\r\n").length).to_eq(std::size_t{18});
    });

    spec::it("waits for the rest of an unfinished request", [] {
      expect(blogin::http::parse_request("GET / HTTP/1.1\r\nHost: ").state ==
             blogin::http::RequestState::incomplete)
        .to_be_true();
    });

    spec::it("refuses a request line with no target", [] {
      expect(blogin::http::parse_request("GET\r\n\r\n").state == blogin::http::RequestState::malformed)
        .to_be_true();
    });

    spec::it("refuses a request line with no version", [] {
      expect(blogin::http::parse_request("GET /\r\n\r\n").state ==
             blogin::http::RequestState::malformed)
        .to_be_true();
    });

    spec::it("refuses a request line with an empty method", [] {
      expect(blogin::http::parse_request(" / HTTP/1.1\r\n\r\n").state ==
             blogin::http::RequestState::malformed)
        .to_be_true();
    });

    spec::it("refuses a request line with an empty target", [] {
      expect(blogin::http::parse_request("GET  HTTP/1.1\r\n\r\n").state ==
             blogin::http::RequestState::malformed)
        .to_be_true();
    });

    spec::it("reads a request that carries no headers at all", [] {
      expect(blogin::http::parse_request("GET / HTTP/1.1\r\n\r\n").request.method).to_eq("GET");
    });

    spec::it("refuses a header with no colon", [] {
      expect(blogin::http::parse_request("GET / HTTP/1.1\r\nrubbish\r\n\r\n").state ==
             blogin::http::RequestState::malformed)
        .to_be_true();
    });

    spec::context("keeping the connection open", [] {
      spec::it("keeps a 1.1 connection open by default", [] {
        expect(parsed("GET / HTTP/1.1\r\n\r\n").wants_keep_alive()).to_be_true();
      });

      spec::it("closes a 1.1 connection that asked to close", [] {
        expect(parsed("GET / HTTP/1.1\r\nConnection: close\r\n\r\n").wants_keep_alive()).to_be_false();
      });

      spec::it("closes a 1.0 connection by default", [] {
        expect(parsed("GET / HTTP/1.0\r\n\r\n").wants_keep_alive()).to_be_false();
      });

      spec::it("keeps a 1.0 connection open when it asked to", [] {
        expect(parsed("GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n").wants_keep_alive())
          .to_be_true();
      });
    });
  });

  spec::describe("writing a response", [] {
    spec::it("writes the status line", [] {
      blogin::http::Response response;
      response.body = "hi";

      expect(blogin::http::serialize(response)).to_contain("HTTP/1.1 200 OK\r\n");
    });

    spec::it("writes the length of the body", [] {
      blogin::http::Response response;
      response.body = "hello";

      expect(blogin::http::serialize(response)).to_contain("Content-Length: 5\r\n");
    });

    spec::it("writes the body after the headers", [] {
      blogin::http::Response response;
      response.body = "hello";

      expect(blogin::http::serialize(response)).to_contain("\r\n\r\nhello");
    });

    // A cached preview response would hide the edit it was rebuilt for.
    spec::it("tells the browser not to cache anything", [] {
      expect(blogin::http::serialize({})).to_contain("Cache-Control: no-store");
    });

    const std::vector<std::pair<int, std::string>> statuses{
      {200, "OK"}, {400, "Bad Request"}, {404, "Not Found"},
      {405, "Method Not Allowed"}, {101, "Switching Protocols"},
    };

    for (const auto& status : statuses) {
      spec::it("names the reason for " + std::to_string(status.first), [status] {
        expect(std::string(blogin::http::reason_for(status.first))).to_eq(status.second);
      });
    }

    spec::it("writes an upgrade without a body or a length", [] {
      blogin::http::Response response;
      response.status = 101;
      response.headers.emplace("Upgrade", "websocket");

      expect(blogin::http::serialize(response)).not_to_contain("Content-Length");
    });
  });

  spec::describe("content types", [] {
    spec::it("names html", [] {
      expect(blogin::http::content_type_for("a/index.html")).to_eq("text/html; charset=utf-8");
    });

    spec::it("names css", [] { expect(blogin::http::content_type_for("a/x.css")).to_eq("text/css"); });

    spec::it("ignores the case of the extension", [] {
      expect(blogin::http::content_type_for("a/PHOTO.PNG")).to_eq("image/png");
    });

    spec::it("falls back rather than guessing", [] {
      expect(blogin::http::content_type_for("a/thing.zzz")).to_eq("application/octet-stream");
    });

    spec::it("falls back for a file with no extension", [] {
      expect(blogin::http::content_type_for("a/CNAME")).to_eq("application/octet-stream");
    });
  });

  spec::describe("resolving a url to a file", [] {
    spec::it("serves the index for the root", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/", root)->filename().string()).to_eq("index.html");
    });

    spec::it("serves a file named outright", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/about.html", root)->filename().string()).to_eq("about.html");
    });

    // What a static host does, and what a site with clean urls publishes.
    spec::it("adds html to an extensionless url", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/about", root)->filename().string()).to_eq("about.html");
    });

    spec::it("serves the index of a directory named without a slash", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/guide", root)->parent_path().filename().string())
        .to_eq("guide");
    });

    spec::it("serves the index of a directory named with a slash", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/guide/", root)->filename().string()).to_eq("index.html");
    });

    spec::it("ignores a query string", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/about?v=2", root)->filename().string()).to_eq("about.html");
    });

    spec::it("finds nothing for a url that names nothing", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/missing", root).has_value()).to_be_false();
    });

    // Checked on the resolved path rather than by looking for "..", which a
    // client can encode its way around.
    spec::it("refuses to reach outside what is being served", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/../outside.txt", root).has_value()).to_be_false();
    });

    spec::it("refuses a directory as a file", [] {
      const std::filesystem::path root = served_tree();

      expect(blogin::http::resolve_file("/assets/css", root).has_value()).to_be_false();
    });
  });
}
