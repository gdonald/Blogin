#pragma once

#include <expected>
#include <string_view>

#include "json.h"
#include "value.h"

namespace blogin {

// A YAML subset: block mappings and sequences, scalars, inline flow sequences,
// comments, and a leading document marker. Anything outside it fails with a
// line number.
std::expected<Value, ParseError> parse_yaml(std::string_view text);

}  // namespace blogin
