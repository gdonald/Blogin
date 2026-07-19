#include "slug.h"

#include <string_view>

#include "text.h"

namespace blogin::slug {
namespace {

bool is_slug_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9');
}

// Characters that carry the whole meaning of a name and cannot be dropped.
// Discarding them turns "c++" into "c", which is not a shortened URL but a
// different tag's URL.
std::string_view spelled_out(char character) {
  switch (character) {
    case '+':
      return "plus";
    case '#':
      return "sharp";
    case '&':
      return "and";
    default:
      return {};
  }
}

}  // namespace

std::string slugify(std::string_view text) {
  const std::string lowered = text::to_lower_ascii(text);

  std::string out;
  bool pending_hyphen = false;

  for (const char character : lowered) {
    if (const std::string_view word = spelled_out(character); !word.empty()) {
      if (!out.empty()) {
        out += '-';
      }

      out += word;
      pending_hyphen = true;

      continue;
    }

    if (is_slug_character(character)) {
      if (pending_hyphen && !out.empty()) {
        out += '-';
      }

      pending_hyphen = false;
      out += character;
      continue;
    }

    pending_hyphen = true;
  }

  return out;
}

std::string humanize(std::string_view text) {
  std::string out;
  bool start_of_word = true;

  for (const char character : text) {
    if (character == '-' || character == '_') {
      if (!out.empty() && !start_of_word) {
        out += ' ';
      }

      start_of_word = true;
      continue;
    }

    if (start_of_word && character >= 'a' && character <= 'z') {
      out += static_cast<char>(character - 'a' + 'A');
    } else {
      out += character;
    }

    start_of_word = false;
  }

  return out;
}

}  // namespace blogin::slug
