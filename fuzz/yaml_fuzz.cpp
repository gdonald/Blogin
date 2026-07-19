#include <cstddef>
#include <cstdint>
#include <string>

#include "json.h"
#include "require.h"
#include "yaml.h"

// Every post carries YAML frontmatter, so this parser reads whatever an author
// wrote on every build.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  const auto parsed = blogin::parse_yaml(input);

  if (!parsed) {
    return 0;
  }

  // Parsing twice has to agree, and whatever came back has to survive being
  // written as JSON, which is how data reaches the search index.
  const auto again = blogin::parse_yaml(input);

  FUZZ_REQUIRE(again.has_value());
  FUZZ_REQUIRE(blogin::to_json(*parsed) == blogin::to_json(*again));

  const std::string written = blogin::to_json(*parsed);

  FUZZ_REQUIRE(blogin::parse_json(written).has_value());

  return 0;
}
