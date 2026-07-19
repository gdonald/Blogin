#include <cstddef>
#include <cstdint>
#include <string>

#include "expression.h"
#include "require.h"
#include "view_context.h"

// The expression language is what a layout writes inside a template, and a
// layout is as much untrusted input as a post is.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  const auto parsed = blogin::expression::parse(input);

  if (!parsed) {
    return 0;
  }

  // Evaluating against a context that knows nothing has to report the unknown
  // name rather than reading whatever a null lookup returned.
  blogin::ViewContext context;

  const auto value = blogin::expression::evaluate(**parsed, context);

  (void)value;

  const auto again = blogin::expression::parse(input);

  FUZZ_REQUIRE(again.has_value());

  return 0;
}
