#pragma once

#include <string>
#include <vector>

namespace spec {

// One example lifted from the CommonMark specification.
struct CommonMarkExample {
  int number = 0;
  std::string section;
  std::string markdown;
  std::string html;
};

// Reads the examples out of specs/commonmark/spec.txt.
//
// The specification carries its own test suite inline: each example sits in a
// fenced block, input and expected output separated by a period, with the
// enclosing section heading naming what it covers.
std::vector<CommonMarkExample> load_commonmark_examples();

// A section-by-section pass rate, printed rather than asserted, so progress is
// visible while the parser is still being built.
struct ConformanceReport {
  struct Section {
    std::string name;
    int passed = 0;
    int total = 0;
  };

  std::vector<Section> sections;
  int passed = 0;
  int total = 0;

  std::string describe() const;
};

}  // namespace spec
