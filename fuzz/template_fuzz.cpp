#include <cstddef>
#include <cstdint>
#include <string>

#include "require.h"
#include "template.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  const blogin::CompiledTemplate compiled = blogin::CompiledTemplate::compile(input);
  const blogin::Context context;

  std::string out;
  render_template(out, compiled, context);

  // Compiling is a pure function of the source, and rendering a pure function
  // of the compiled form and the context. Either one disagreeing with itself
  // means state leaked out of the first pass.
  const blogin::CompiledTemplate again = blogin::CompiledTemplate::compile(input);

  std::string repeat;
  render_template(repeat, again, context);

  FUZZ_REQUIRE(out == repeat);

  return 0;
}
