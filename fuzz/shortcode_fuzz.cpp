#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "require.h"
#include "shortcode.h"

// Shortcode arguments come out of post bodies, quoting and all.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  const std::vector<blogin::shortcode::Argument> arguments =
    blogin::shortcode::parse_arguments(input);

  // Looking a name up has to give back the first argument written with it. An
  // author who repeats a name gets the first one, and a lookup that disagreed
  // with a scan would mean a shortcode reading a value nobody wrote.
  for (const blogin::shortcode::Argument& entry : arguments) {
    if (entry.name.empty()) {
      continue;
    }

    const auto first = std::find_if(
      arguments.begin(), arguments.end(),
      [&entry](const blogin::shortcode::Argument& candidate) { return candidate.name == entry.name; });

    FUZZ_REQUIRE(first != arguments.end());
    FUZZ_REQUIRE(blogin::shortcode::argument(arguments, entry.name) == first->value);
  }

  blogin::shortcode::render_template(input, arguments);

  if (blogin::shortcode::is_builtin(input)) {
    blogin::shortcode::expand_builtin(input, arguments);
  }

  return 0;
}
