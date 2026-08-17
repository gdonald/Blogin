#include "search.h"

#include <algorithm>
#include <format>

#include "json.h"
#include "text.h"

namespace blogin::search {
namespace {

// The browser script and this ranking have to agree, so the weights live in the
// header and are written into the script, never repeated by hand.
constexpr std::string_view search_js = R"JS((function () {
  const WEIGHT = { title: 10, tag: 5, body: 1 };
  const CAP = (typeof BLOGIN_SEARCH_CAP === 'number') ? BLOGIN_SEARCH_CAP : 10;

  const form = document.querySelector('[data-blogin-search]');
  if (!form) return;

  const input = form.querySelector('input');
  const results = document.querySelector('[data-blogin-results]');

  // Float the results on <body> so no ancestor stacking context (a card with
  // backdrop-filter, a transformed container) can trap them behind the content
  // below. Positioned at the input each time they are shown.
  if (results && results.parentNode !== document.body) document.body.appendChild(results);

  function place() {
    const r = input.getBoundingClientRect();
    results.style.top = (r.bottom + 4) + 'px';
    results.style.left = r.left + 'px';
    results.style.width = r.width + 'px';
  }

  let records = [];
  fetch('/search-index.json').then(function (r) { return r.json(); }).then(function (data) {
    records = data;
  });

  function tokenize(text) {
    return (text.toLowerCase().match(/\w+/g) || []);
  }

  function wordCount(text, token) {
    return tokenize(text).filter(function (w) { return w.indexOf(token) === 0; }).length;
  }

  function rank(query) {
    const tokens = tokenize(query);
    if (!tokens.length) return [];

    const scored = [];
    for (const rec of records) {
      let score = 0;
      for (const t of tokens) {
        score += WEIGHT.title * wordCount(rec.title || '', t);
        score += WEIGHT.tag * (rec.tags || []).map(function (x) { return x.toLowerCase(); })
          .filter(function (x) { return x.indexOf(t) === 0; }).length;
        score += WEIGHT.body * wordCount(rec.text || '', t);
      }
      if (score > 0) scored.push({ rec: rec, score: score });
    }

    scored.sort(function (a, b) {
      return (b.score - a.score) || a.rec.title.localeCompare(b.rec.title);
    });

    return scored.slice(0, CAP).map(function (s) { return s.rec; });
  }

  function snippet(text, tokens) {
    const lower = text.toLowerCase();
    for (const t of tokens) {
      const i = lower.indexOf(t);
      if (i >= 0) {
        const start = Math.max(0, i - 30);
        return (start > 0 ? '…' : '') + text.slice(start, i + 60) + '…';
      }
    }
    return text.slice(0, 90);
  }

  function render(query) {
    results.innerHTML = '';
    const tokens = tokenize(query);
    for (const rec of rank(query)) {
      const li = document.createElement('li');

      const a = document.createElement('a');
      a.href = rec.url;

      const title = document.createElement('span');
      title.className = 'blogin-result-title';
      title.textContent = rec.title;
      a.appendChild(title);

      const p = document.createElement('p');
      p.textContent = snippet(rec.text || '', tokens);
      a.appendChild(p);

      li.appendChild(a);
      results.appendChild(li);
    }
    if (results.children.length) place();
  }

  input.addEventListener('input', function () { render(input.value); });
  window.addEventListener('resize', function () { if (results.children.length) place(); });
  window.addEventListener('scroll', function () { if (results.children.length) place(); }, true);
})();
)JS";

constexpr std::string_view search_css = R"CSS([data-blogin-search] {
  margin: 0;
}

[data-blogin-search] input {
  width: 100%;
  min-width: 12rem;
  box-sizing: border-box;
  padding: 0.5rem 0.75rem;
  font-size: 1rem;
  line-height: 1.5;
  color: #212529;
  background: #fff;
  border: 1px solid #ced4da;
  border-radius: 0.5rem;
}

[data-blogin-search] input:focus {
  outline: none;
  border-color: #86b7fe;
  box-shadow: 0 0 0 0.25rem rgba(13, 110, 253, 0.25);
}

.blogin-search {
  position: relative;
}

[data-blogin-results]:empty {
  display: none;
}

[data-blogin-results] {
  position: fixed;
  z-index: 1050;
  margin: 0;
  padding: 0;
  list-style: none;
  max-height: 70vh;
  overflow: hidden auto;
  background: #fff;
  border: 1px solid rgba(0, 0, 0, 0.15);
  border-radius: 0.5rem;
  box-shadow: 0 0.5rem 1rem rgba(0, 0, 0, 0.15);
}

[data-blogin-results] li + li {
  border-top: 1px solid #f0f0f0;
}

[data-blogin-results] a {
  display: block;
  padding: 0.5rem 0.75rem;
  text-decoration: none;
  color: inherit;
  overflow-wrap: break-word;
}

[data-blogin-results] a:hover {
  background: #f6f8fa;
}

[data-blogin-results] .blogin-result-title {
  display: block;
  font-weight: 600;
}

[data-blogin-results] p {
  margin: 0.25rem 0 0;
  font-size: 0.8125rem;
  color: #6c757d;
}
)CSS";

bool is_word_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '_';
}

int count_prefixed(std::string_view text, std::string_view token) {
  int matches = 0;

  for (const std::string& word : tokenize(text)) {
    if (word.starts_with(token)) {
      ++matches;
    }
  }

  return matches;
}

}  // namespace

std::vector<std::string> tokenize(std::string_view text) {
  std::vector<std::string> tokens;
  std::size_t index = 0;

  while (index < text.size()) {
    while (index < text.size() && !is_word_character(text[index])) {
      ++index;
    }

    const std::size_t start = index;

    while (index < text.size() && is_word_character(text[index])) {
      ++index;
    }

    if (index > start) {
      tokens.push_back(text::to_lower_ascii(text.substr(start, index - start)));
    }
  }

  return tokens;
}

int score(const SearchRecord& record, const std::vector<std::string>& tokens) {
  int total = 0;

  for (const std::string& token : tokens) {
    total += title_weight * count_prefixed(record.title, token);
    total += body_weight * count_prefixed(record.text, token);

    for (const std::string& tag : record.tags) {
      if (text::to_lower_ascii(tag).starts_with(token)) {
        total += tag_weight;
      }
    }
  }

  return total;
}

std::vector<SearchRecord> rank(const std::vector<SearchRecord>& records, std::string_view query,
                               std::size_t cap) {
  const std::vector<std::string> tokens = tokenize(query);

  if (tokens.empty()) {
    return {};
  }

  std::vector<std::pair<int, const SearchRecord*>> scored;

  for (const SearchRecord& record : records) {
    if (const int value = score(record, tokens); value > 0) {
      scored.emplace_back(value, &record);
    }
  }

  std::stable_sort(scored.begin(), scored.end(), [](const auto& left, const auto& right) {
    return left.first != right.first ? left.first > right.first : left.second->title < right.second->title;
  });

  std::vector<SearchRecord> best;

  for (std::size_t index = 0; index < scored.size() && index < cap; ++index) {
    best.push_back(*scored[index].second);
  }

  return best;
}

std::string index_json(const std::vector<SearchRecord>& records, std::size_t text_length) {
  Value list = Value::array();

  for (const SearchRecord& record : records) {
    Value tags = Value::array();

    for (const std::string& tag : record.tags) {
      tags.push(Value(tag));
    }

    Value entry = Value::object();
    entry.set("title", Value(record.title));
    entry.set("url", Value(record.url));
    entry.set("date", Value(record.date));
    entry.set("description", Value(record.description));
    entry.set("tags", std::move(tags));
    entry.set("text", Value(std::string(text::substr(record.text, 0, text_length))));

    list.push(std::move(entry));
  }

  return to_json(list, JsonStyle::compact, true);
}

std::string script(int cap) {
  return std::format("const BLOGIN_SEARCH_CAP = {};\n", cap) + std::string(search_js);
}

std::string_view stylesheet() {
  return search_css;
}

}  // namespace blogin::search
