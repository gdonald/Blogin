#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace blogin::http {

// What a request line and headers said, with the body left where it was.
//
// Parsing is separate from the socket so a spec can hand it bytes without
// arrange a connection.
struct Request {
  std::string method;

  // The target as written, query string and all.
  std::string target;

  // The target with the query removed, the part that resolves to a file.
  std::string path;

  std::string version;

  // Lowercased names, since a client may write them however it likes.
  std::map<std::string, std::string> headers;

  // How many bytes of the buffer the request occupied, so a keep-alive
  // connection knows where the next one starts.
  std::size_t length = 0;

  std::string_view header(std::string_view name) const;

  bool wants_keep_alive() const;
};

// Nothing when the buffer does not yet hold a whole request. An unparseable one
// is reported.
enum class RequestState {
  incomplete,
  ready,
  malformed,
};

struct ParsedRequest {
  RequestState state = RequestState::incomplete;
  Request request;
};

ParsedRequest parse_request(std::string_view buffer);

struct Response {
  int status = 200;
  std::string content_type = "text/plain";
  std::string body;
  bool keep_alive = true;

  // Extra headers, for the WebSocket upgrade.
  std::map<std::string, std::string> headers;
};

// The response as bytes, headers and all.
std::string serialize(const Response& response);

std::string_view reason_for(int status);

// By extension, falling back to octet-stream.
std::string content_type_for(const std::filesystem::path& file);

// The file a request path names, mirroring how a static host rewrites an
// extensionless url. Nothing when the path resolves to no file, or when it
// reaches outside the root.
std::optional<std::filesystem::path> resolve_file(std::string_view url_path,
                                                  const std::filesystem::path& root);

}  // namespace blogin::http
