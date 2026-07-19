#pragma once

// Shared between the block and inline halves of the parser. Not public API, so
// it sits beside the implementation rather than in the include directory.

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "arena.h"
#include "markdown.h"

namespace blogin::markdown_detail {

struct Reference {
  std::string_view url;
  std::string_view title;
};

using ReferenceMap = std::unordered_map<std::string, Reference>;

// Footnote bodies, keyed by label, as raw markdown to be parsed once the whole
// document is known.
using FootnoteMap = std::unordered_map<std::string, std::string_view>;

bool is_space_or_tab(char character);
bool is_digit(char character);
bool is_letter(char character);
bool is_blank(std::string_view line);
bool is_punctuation(char character);
bool is_whitespace(char character);

// Case-folded, whitespace-collapsed form used to match a reference to its
// definition.
std::string normalize_label(std::string_view label);

void append_utf8(std::string& out, std::uint32_t code_point);

// Bytes consumed by an entity reference at the start of `text`, zero when it is
// not one.
std::size_t decode_entity(std::string_view input, std::string& out);

// Decodes the code point starting at `offset`, and the one ending just before
// it. Emphasis flanking is defined over characters, not bytes.
std::uint32_t code_point_at(std::string_view text, std::size_t offset);
std::uint32_t code_point_before(std::string_view text, std::size_t offset);

// Whitespace and punctuation as the emphasis rules mean them, which is wider
// than ASCII: a non-breaking space is whitespace and a currency sign is
// punctuation, and both change whether a delimiter run can open or close.
bool is_unicode_whitespace(std::uint32_t code_point);
bool is_unicode_punctuation(std::uint32_t code_point);

// Length of a complete open or closing HTML tag at the start of `text`, zero
// when there is not one. Shared so that block detection and inline scanning
// agree on what a tag is.
std::size_t match_html_tag(std::string_view text);

// Resolves backslash escapes and entity references.
std::string unescape_string(std::string_view input);

// Reads one link reference definition from the front of `text`, recording it.
// Returns how many bytes it consumed, zero when there is not one.
std::size_t parse_reference_definition(Arena& arena, std::string_view input, ReferenceMap& references);

// Reads one footnote definition, "[^label]: body", from the front of `text`.
std::size_t parse_footnote_definition(Arena& arena, std::string_view input, FootnoteMap& footnotes);

// Numbers footnote references in first-reference order and builds the section
// that holds their bodies. Returns the section, or null when nothing referred
// to a definition.
Node* resolve_footnotes(Arena& arena, Node* document, const FootnoteMap& footnotes,
                        const ReferenceMap& references);

// Walks the block tree and turns every paragraph and heading literal into
// inline children.
void parse_inlines(Arena& arena, Node* root, const ReferenceMap& references);

}  // namespace blogin::markdown_detail
