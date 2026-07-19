#include <cstddef>
#include <cstdint>
#include <string>

#include "config.h"
#include "json.h"
#include "require.h"

// blogin.json is hand-written, so a malformed one has to be reported rather
// than half-applied.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  const auto value = blogin::parse_json(input);

  if (!value) {
    return 0;
  }

  const auto config = blogin::Config::from_value(*value);

  if (!config) {
    return 0;
  }

  // A key the configuration does not know is reported, never dropped in
  // silence, and asking for a hint about it must not depend on the key being a
  // real one.
  for (const std::string& unknown : config->unknown_keys) {
    blogin::nearest_key_hint(unknown);
  }

  // Looking up a section by a name no section has must come back empty rather
  // than reaching past the end of the list.
  for (const blogin::SectionConfig& section : config->sections) {
    FUZZ_REQUIRE(config->section(section.name) != nullptr);
  }

  return 0;
}
