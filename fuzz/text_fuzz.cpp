#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "require.h"
#include "text.h"

namespace {

// Every helper here hands back a view rather than a copy, so the question each
// invariant asks is whether the view still points inside the string it came
// from. A view that has slipped past the end reads memory the post does not
// own, and the summary, the search index, and the table of contents are all
// built out of these.
void require_inside(std::string_view piece, std::string_view whole) {
  // "Nothing here" comes back as a default-constructed view, which holds no
  // pointer to compare. Anything with bytes in it has to point at the original.
  if (piece.empty()) {
    return;
  }

  FUZZ_REQUIRE(piece.data() >= whole.data());
  FUZZ_REQUIRE(piece.data() + piece.size() <= whole.data() + whole.size());
}

}  // namespace

// Post bodies are UTF-8 and nothing guarantees they are valid UTF-8. Indexing
// by character over bytes is where a truncated sequence at the end of a buffer
// turns into a read past it.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  const std::string_view text = input;

  const std::size_t characters = blogin::text::char_length(text);

  // One character is at least one byte, so there can never be more characters
  // than bytes.
  FUZZ_REQUIRE(characters <= text.size());

  // Asking for the byte at any character index, including one past the last,
  // has to land inside the buffer or exactly at its end.
  for (std::size_t index = 0; index <= characters; ++index) {
    FUZZ_REQUIRE(blogin::text::byte_offset(text, index) <= text.size());
  }

  // And an index beyond the end has to clamp rather than run on.
  FUZZ_REQUIRE(blogin::text::byte_offset(text, characters + 1) <= text.size());
  FUZZ_REQUIRE(blogin::text::byte_offset(text, text.size() + 64) <= text.size());

  require_inside(blogin::text::trim(text), text);
  require_inside(blogin::text::trim_start(text), text);
  require_inside(blogin::text::trim_end(text), text);

  // The summary takes a character range out of the middle of a post, which is
  // the call that has to get UTF-8 boundaries right.
  if (characters > 0) {
    require_inside(blogin::text::substr(text, 0, characters), text);
    require_inside(blogin::text::substr(text, characters / 2, characters), text);
    require_inside(blogin::text::substr(text, characters - 1, 1), text);
  }

  require_inside(blogin::text::substr(text, characters + 8, 4), text);

  for (const std::string_view line : blogin::text::split_lines(text)) {
    require_inside(line, text);
  }

  for (const std::string_view piece : blogin::text::split(text, ',')) {
    require_inside(piece, text);
  }

  for (const std::string_view word : blogin::text::words(text)) {
    require_inside(word, text);
  }

  FUZZ_REQUIRE(blogin::text::word_count(text) == blogin::text::words(text).size());

  // ASCII case folding touches one byte at a time and must not change how many
  // there are, which is what keeps a byte offset taken before it still valid.
  FUZZ_REQUIRE(blogin::text::to_lower_ascii(text).size() == text.size());
  FUZZ_REQUIRE(blogin::text::to_upper_ascii(text).size() == text.size());

  return 0;
}
