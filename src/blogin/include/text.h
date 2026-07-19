#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blogin {

// UTF-8 aware string handling.
//
// std::string counts bytes, but slugging, summary truncation, and word counting
// all mean characters. A post with accented text has to truncate where a reader
// would expect rather than in the middle of a multi-byte sequence.
//
// Malformed input is never rejected. A byte that cannot begin a valid sequence
// counts as one character and is copied through, because a blog post with one
// bad byte should still build.
namespace text {

std::size_t char_length(std::string_view text);

// Byte offset of a character index, clamped to the end of the string.
std::size_t byte_offset(std::string_view text, std::size_t char_index);

// Substring by character index, not byte index. Never splits a sequence.
std::string_view substr(std::string_view text, std::size_t char_start,
                        std::size_t char_count = std::string_view::npos);

std::size_t sequence_length(unsigned char lead);

bool is_continuation(unsigned char byte);

std::string_view trim(std::string_view text);
std::string_view trim_start(std::string_view text);
std::string_view trim_end(std::string_view text);

std::vector<std::string_view> split(std::string_view text, char separator);

std::vector<std::string_view> split_lines(std::string_view text);

// Whitespace-separated runs, which is what a word count means here.
std::vector<std::string_view> words(std::string_view text);

std::size_t word_count(std::string_view text);

// ASCII only, deliberately. Case folding the rest of Unicode needs tables this
// project does not carry, and every place that folds case here is matching
// ASCII keywords or building slugs.
std::string to_lower_ascii(std::string_view text);
std::string to_upper_ascii(std::string_view text);

bool starts_with(std::string_view text, std::string_view prefix);
bool ends_with(std::string_view text, std::string_view suffix);

bool is_space(char character);

// Reads a floating-point number, all of the text or none of it. Nothing comes
// back for trailing characters, for an empty string, or for a value too large
// or too small to hold.
//
// `std::from_chars` over a double is what this would otherwise be. libc++
// implemented that late, and Apple's copy marks it unavailable below macOS 26,
// which would put the deployment floor above every machine anyone is running.
// `strtod` has been in C since C89. It reads the decimal point according to the
// locale, and this program never calls `setlocale`, so the point is a point.
std::optional<double> to_double(std::string_view text);

}  // namespace text
}  // namespace blogin
