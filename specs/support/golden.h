#pragma once

#include <source_location>
#include <string>
#include <string_view>

namespace spec {

// Compares against a committed file under specs/golden/. On a mismatch the
// failure carries a unified diff.
//
// Regeneration is always explicit, through scripts/regold.sh, which sets
// BLOGIN_REGOLD. A golden file never rewrites itself out from under a failing
// example.
void expect_golden(std::string_view name, std::string_view actual,
                   std::source_location where = std::source_location::current());

bool regenerating_goldens();

std::string unified_diff(std::string_view expected, std::string_view actual);

}  // namespace spec
