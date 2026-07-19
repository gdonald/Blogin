#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "haml.h"
#include "require.h"
#include "view_context.h"

// Layouts are HAML, and a theme is something a reader downloads and drops in,
// so the compiler reads bytes nobody on this side wrote.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  auto compiled = blogin::haml::Template::compile(input, "fuzz");

  if (!compiled) {
    // A source the compiler rejects has to be rejected the same way every time.
    // A compiler that accepts on the second pass is carrying state across.
    FUZZ_REQUIRE(!blogin::haml::Template::compile(input, "fuzz").has_value());

    return 0;
  }

  blogin::ViewContext context;

  blogin::haml::RenderOptions options;

  // Without a lookup the engine calls an empty std::function. Reporting a
  // missing partial is the behaviour under test, not throwing bad_function_call.
  options.partial = [](std::string_view) -> const blogin::haml::Template* { return nullptr; };

  const auto rendered = blogin::haml::render(*compiled, context, options);

  auto again = blogin::haml::Template::compile(input, "fuzz");

  FUZZ_REQUIRE(again.has_value());

  blogin::ViewContext fresh;
  const auto repeat = blogin::haml::render(*again, fresh, options);

  FUZZ_REQUIRE(rendered.has_value() == repeat.has_value());

  if (rendered && repeat) {
    FUZZ_REQUIRE(*rendered == *repeat);
  }

  return 0;
}
