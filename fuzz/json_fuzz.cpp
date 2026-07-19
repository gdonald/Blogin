#include <cstddef>
#include <cstdint>
#include <string>

#include "json.h"
#include "require.h"
#include "value.h"

// Data files are user-authored, so this parser reads untrusted bytes on every
// build, not only when a preview server is running.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  const auto parsed = blogin::parse_json(input);

  if (!parsed) {
    return 0;
  }

  // Serializing and reparsing has to land on the same value. A round trip that
  // loses or changes something is a data file that builds into the wrong page,
  // which no crash would have revealed.
  const std::string written = blogin::to_json(*parsed);
  const auto reparsed = blogin::parse_json(written);

  FUZZ_REQUIRE(reparsed.has_value());
  FUZZ_REQUIRE(blogin::to_json(*reparsed) == written);

  // Sorting keys changes the order, never the content.
  const std::string sorted = blogin::to_json(*parsed, blogin::JsonStyle::compact, true);
  const auto from_sorted = blogin::parse_json(sorted);

  FUZZ_REQUIRE(from_sorted.has_value());

  return 0;
}
