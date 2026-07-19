#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "http.h"
#include "require.h"

// The preview server listens on a socket, so these bytes arrive from whoever
// can reach it rather than from the site being built.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string buffer(reinterpret_cast<const char*>(data), size);

  const blogin::http::ParsedRequest parsed = blogin::http::parse_request(buffer);

  if (parsed.state != blogin::http::RequestState::ready) {
    return 0;
  }

  // length is where the next request on a keep-alive connection starts. One
  // past the end of the buffer desynchronises the connection, and one that
  // stays put loops forever on the same bytes.
  FUZZ_REQUIRE(parsed.request.length <= buffer.size());
  FUZZ_REQUIRE(parsed.request.length > 0);

  // path is target with the query removed, so it can never be the longer of
  // the two and can never name bytes the target did not.
  FUZZ_REQUIRE(parsed.request.path.size() <= parsed.request.target.size());
  FUZZ_REQUIRE(parsed.request.target.starts_with(parsed.request.path));

  // Header names are lowercased on the way in, so a lookup by the lowercase
  // name has to find what the map holds.
  for (const auto& [name, value] : parsed.request.headers) {
    FUZZ_REQUIRE(parsed.request.header(name) == value);
  }

  parsed.request.wants_keep_alive();

  // Handing the same bytes back has to produce the same answer.
  const blogin::http::ParsedRequest repeat = blogin::http::parse_request(buffer);

  FUZZ_REQUIRE(repeat.state == parsed.state);
  FUZZ_REQUIRE(repeat.request.length == parsed.request.length);
  FUZZ_REQUIRE(repeat.request.target == parsed.request.target);

  return 0;
}
