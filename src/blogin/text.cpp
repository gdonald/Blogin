#include "text.h"

#include <cerrno>
#include <cstdlib>

namespace blogin::text {
namespace {

constexpr std::string_view whitespace = " \t\r\n\f\v";

}  // namespace

bool is_continuation(unsigned char byte) {
  return (byte & 0xC0U) == 0x80U;
}

std::size_t sequence_length(unsigned char lead) {
  if (lead < 0x80U) {
    return 1;
  }

  if ((lead & 0xE0U) == 0xC0U) {
    return 2;
  }

  if ((lead & 0xF0U) == 0xE0U) {
    return 3;
  }

  if ((lead & 0xF8U) == 0xF0U) {
    return 4;
  }

  // A continuation byte or an invalid lead. Counted as one character so that
  // malformed input still advances rather than looping.
  return 1;
}

std::size_t char_length(std::string_view text) {
  std::size_t characters = 0;
  std::size_t position = 0;

  while (position < text.size()) {
    position += sequence_length(static_cast<unsigned char>(text[position]));
    ++characters;
  }

  return characters;
}

std::size_t byte_offset(std::string_view text, std::size_t char_index) {
  std::size_t characters = 0;
  std::size_t position = 0;

  while (position < text.size() && characters < char_index) {
    position += sequence_length(static_cast<unsigned char>(text[position]));
    ++characters;
  }

  return position > text.size() ? text.size() : position;
}

std::string_view substr(std::string_view text, std::size_t char_start, std::size_t char_count) {
  const std::size_t start = byte_offset(text, char_start);

  if (char_count == std::string_view::npos) {
    return text.substr(start);
  }

  const std::size_t end = byte_offset(text, char_start + char_count);

  return text.substr(start, end - start);
}

bool is_space(char character) {
  return whitespace.contains(character);
}

std::string_view trim_start(std::string_view text) {
  const auto first = text.find_first_not_of(whitespace);

  return first == std::string_view::npos ? std::string_view{} : text.substr(first);
}

std::string_view trim_end(std::string_view text) {
  const auto last = text.find_last_not_of(whitespace);

  return last == std::string_view::npos ? std::string_view{} : text.substr(0, last + 1);
}

std::string_view trim(std::string_view text) {
  return trim_end(trim_start(text));
}

std::vector<std::string_view> split(std::string_view text, char separator) {
  std::vector<std::string_view> pieces;
  std::size_t start = 0;

  while (true) {
    const auto found = text.find(separator, start);

    if (found == std::string_view::npos) {
      pieces.push_back(text.substr(start));
      break;
    }

    pieces.push_back(text.substr(start, found - start));
    start = found + 1;
  }

  return pieces;
}

std::vector<std::string_view> split_lines(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;

  while (start <= text.size()) {
    const auto newline = text.find('\n', start);

    if (newline == std::string_view::npos) {
      if (start < text.size()) {
        lines.push_back(text.substr(start));
      }

      break;
    }

    std::string_view line = text.substr(start, newline - start);

    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }

    lines.push_back(line);
    start = newline + 1;
  }

  return lines;
}

std::vector<std::string_view> words(std::string_view text) {
  std::vector<std::string_view> found;
  std::size_t position = 0;

  while (position < text.size()) {
    while (position < text.size() && is_space(text[position])) {
      ++position;
    }

    if (position >= text.size()) {
      break;
    }

    const std::size_t start = position;

    while (position < text.size() && !is_space(text[position])) {
      ++position;
    }

    found.push_back(text.substr(start, position - start));
  }

  return found;
}

std::size_t word_count(std::string_view text) {
  return words(text).size();
}

std::string to_lower_ascii(std::string_view text) {
  std::string out(text);

  for (char& character : out) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }

  return out;
}

std::string to_upper_ascii(std::string_view text) {
  std::string out(text);

  for (char& character : out) {
    if (character >= 'a' && character <= 'z') {
      character = static_cast<char>(character - 'a' + 'A');
    }
  }

  return out;
}

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.starts_with(prefix);
}

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.ends_with(suffix);
}

std::optional<double> to_double(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }

  // strtod reads a C string, and a string_view carries no terminator.
  const std::string buffer(text);

  errno = 0;

  char* end = nullptr;
  const double value = std::strtod(buffer.c_str(), &end);

  if (end != buffer.c_str() + buffer.size()) {
    return std::nullopt;
  }

  if (errno == ERANGE) {
    return std::nullopt;
  }

  return value;
}

}  // namespace blogin::text
