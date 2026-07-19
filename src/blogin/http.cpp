#include "http.h"

#include <format>

#include "text.h"

namespace blogin::http {
namespace {

// A request line and its headers, and nothing beyond what is needed to serve a
// preview: no chunked bodies, no continuation lines, no trailers.
constexpr std::size_t maximum_request_size = std::size_t{64} * 1024;

std::string_view trim(std::string_view text) {
  return blogin::text::trim(text);
}

}  // namespace

std::string_view Request::header(std::string_view name) const {
  const auto found = headers.find(blogin::text::to_lower_ascii(name));

  return found == headers.end() ? std::string_view{} : std::string_view(found->second);
}

bool Request::wants_keep_alive() const {
  const std::string connection = blogin::text::to_lower_ascii(header("connection"));

  if (connection.contains("close")) {
    return false;
  }

  if (connection.contains("keep-alive")) {
    return true;
  }

  // HTTP/1.1 keeps the connection open unless told otherwise; 1.0 is the other
  // way round.
  return version == "HTTP/1.1";
}

ParsedRequest parse_request(std::string_view buffer) {
  ParsedRequest parsed;

  const auto end_of_headers = buffer.find("\r\n\r\n");

  if (end_of_headers == std::string_view::npos) {
    // A client that keeps talking without finishing a request is not going to
    // finish one.
    parsed.state = buffer.size() > maximum_request_size ? RequestState::malformed
                                                        : RequestState::incomplete;

    return parsed;
  }

  const std::string_view head = buffer.substr(0, end_of_headers);
  const auto end_of_line = head.find("\r\n");
  const std::string_view line = head.substr(0, end_of_line == std::string_view::npos ? head.size()
                                                                                    : end_of_line);

  const auto first_space = line.find(' ');

  if (first_space == std::string_view::npos) {
    parsed.state = RequestState::malformed;

    return parsed;
  }

  const auto second_space = line.find(' ', first_space + 1);

  if (second_space == std::string_view::npos) {
    parsed.state = RequestState::malformed;

    return parsed;
  }

  Request request;
  request.method = std::string(line.substr(0, first_space));
  request.target = std::string(line.substr(first_space + 1, second_space - first_space - 1));
  request.version = std::string(trim(line.substr(second_space + 1)));

  const auto query = request.target.find('?');
  request.path = query == std::string::npos ? request.target : request.target.substr(0, query);

  if (request.method.empty() || request.target.empty()) {
    parsed.state = RequestState::malformed;

    return parsed;
  }

  if (end_of_line != std::string_view::npos) {
    for (const std::string_view header : blogin::text::split(head.substr(end_of_line + 2), '\n')) {
      const std::string_view stripped = trim(header);

      if (stripped.empty()) {
        continue;
      }

      const auto colon = stripped.find(':');

      if (colon == std::string_view::npos) {
        parsed.state = RequestState::malformed;

        return parsed;
      }

      request.headers.emplace(blogin::text::to_lower_ascii(stripped.substr(0, colon)),
                              std::string(trim(stripped.substr(colon + 1))));
    }
  }

  request.length = end_of_headers + 4;

  parsed.state = RequestState::ready;
  parsed.request = std::move(request);

  return parsed;
}

std::string_view reason_for(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 101:
      return "Switching Protocols";
    default:
      return "OK";
  }
}

std::string serialize(const Response& response) {
  std::string out =
    std::format("HTTP/1.1 {} {}\r\n", response.status, reason_for(response.status));

  if (response.status != 101) {
    out += std::format("Content-Type: {}\r\n", response.content_type);
    out += std::format("Content-Length: {}\r\n", response.body.size());

    // A preview must never be cached, or an edit does not reach the page.
    out += "Cache-Control: no-store\r\n";
    out += std::format("Connection: {}\r\n", response.keep_alive ? "keep-alive" : "close");
  }

  for (const auto& header : response.headers) {
    out += std::format("{}: {}\r\n", header.first, header.second);
  }

  out += "\r\n";

  if (response.status != 101) {
    out += response.body;
  }

  return out;
}

std::string content_type_for(const std::filesystem::path& file) {
  static const std::map<std::string, std::string> types{
    {"html", "text/html; charset=utf-8"},
    {"css", "text/css"},
    {"js", "application/javascript"},
    {"json", "application/json"},
    {"xml", "application/xml"},
    {"svg", "image/svg+xml"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"gif", "image/gif"},
    {"webp", "image/webp"},
    {"ico", "image/x-icon"},
    {"txt", "text/plain"},
  };

  const std::string extension = blogin::text::to_lower_ascii(file.extension().string());

  if (extension.empty()) {
    return "application/octet-stream";
  }

  const auto found = types.find(extension.substr(1));

  return found == types.end() ? "application/octet-stream" : found->second;
}

std::optional<std::filesystem::path> resolve_file(std::string_view url_path,
                                                  const std::filesystem::path& root) {
  std::string relative(url_path);

  if (const auto query = relative.find('?'); query != std::string::npos) {
    relative.resize(query);
  }

  if (relative.starts_with('/')) {
    relative.erase(0, 1);
  }

  if (relative.empty() || relative.ends_with('/')) {
    relative += "index.html";
  }

  // A request is not allowed to reach outside what is being served, whatever it
  // spells. This is checked on the resolved path rather than by looking for
  // "..", which a client can encode around.
  const auto within = [&root](const std::filesystem::path& candidate)
    -> std::optional<std::filesystem::path> {
    std::error_code error;

    if (!std::filesystem::is_regular_file(candidate, error)) {
      return std::nullopt;
    }

    std::filesystem::path resolved = std::filesystem::weakly_canonical(candidate, error);
    const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);

    if (error) {
      return std::nullopt;
    }

    const auto relation = resolved.lexically_relative(base);

    if (relation.empty() || *relation.begin() == "..") {
      return std::nullopt;
    }

    return resolved;
  };

  if (const auto direct = within(root / relative)) {
    return direct;
  }

  // An extensionless url is what a clean-url site publishes, and a static host
  // resolves it either way.
  if (!relative.contains('.')) {
    if (const auto page = within(root / (relative + ".html"))) {
      return page;
    }

    if (const auto index = within(root / relative / "index.html")) {
      return index;
    }
  }

  return std::nullopt;
}

}  // namespace blogin::http
