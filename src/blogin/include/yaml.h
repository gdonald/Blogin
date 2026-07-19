#pragma once

#include <expected>
#include <string_view>

#include "json.h"
#include "value.h"

namespace blogin {

// A YAML subset, not YAML.
//
// Data files in practice use block mappings, block sequences, scalars, quoted
// strings, and comments. Full YAML 1.2 is a project of its own: anchors,
// aliases, tags, multiple documents, flow collections nested arbitrarily, block
// scalar folding rules, and a type-resolution system.
//
// What is supported:
//   - block mappings, nested by indentation
//   - block sequences, including sequences of mappings
//   - plain, single-quoted, and double-quoted scalars
//   - inline flow sequences, [a, b, c]
//   - null, true, false, integers, and numbers, resolved as in JSON
//   - comments, from an unquoted # to end of line
//   - a leading --- document marker
//
// Anything else fails with a line number rather than being guessed at.
std::expected<Value, ParseError> parse_yaml(std::string_view text);

}  // namespace blogin
