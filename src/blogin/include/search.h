#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "value.h"

namespace blogin {

// One post as the browser search sees it.
struct SearchRecord {
  std::string title;
  std::string url;
  std::string date;
  std::string description;
  std::string text;
  std::vector<std::string> tags;
};

namespace search {

// Weights, mirrored in the emitted script. A title match counts for far more
// than a body match, so a short index still ranks well.
inline constexpr int title_weight = 10;
inline constexpr int tag_weight = 5;
inline constexpr int body_weight = 1;

std::vector<std::string> tokenize(std::string_view text);

int score(const SearchRecord& record, const std::vector<std::string>& tokens);

// Best matches first, ties broken by title so the order is stable.
std::vector<SearchRecord> rank(const std::vector<SearchRecord>& records, std::string_view query,
                               std::size_t cap = 10);

// The index the browser fetches. Keys are sorted so the file is stable whatever
// order posts were discovered in.
std::string index_json(const std::vector<SearchRecord>& records, std::size_t text_length = 2000);

// The script and stylesheet the search box needs, emitted into the site.
std::string script(int cap = 10);

std::string_view stylesheet();

}  // namespace search
}  // namespace blogin
