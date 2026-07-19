#include "highlight.h"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>

#include "text.h"

namespace blogin::highlight {
namespace {

struct Language {
  std::string_view comment;
  std::unordered_set<std::string_view> keywords;
};

std::unordered_set<std::string_view> words_of(std::string_view list) {
  std::unordered_set<std::string_view> words;

  for (const std::string_view word : text::words(list)) {
    words.insert(word);
  }

  return words;
}

const std::unordered_map<std::string_view, Language>& languages_by_name() {
  static const std::unordered_map<std::string_view, Language> known{
    {"raku", {"#", words_of("my our sub method class role grammar token rule regex has is does return if "
                            "elsif else unless for while loop given when default use need unit module multi "
                            "proto enum constant")}},
    {"ruby", {"#", words_of("def end class module if elsif else unless case when while until do return yield "
                            "require attr_accessor new self nil true false")}},
    {"python", {"#", words_of("def class if elif else for while return import from as with try except finally "
                              "lambda pass yield None True False and or not in is")}},
    {"javascript", {"//", words_of("function var let const if else for while return class new this import "
                                   "export from async await try catch throw typeof of")}},
    {"bash", {"#", words_of("if then elif else fi for in do done while until case esac function echo return "
                            "export local")}},
    {"json", {"", {}}},
    {"c", {"//", words_of("int char short long float double void unsigned signed const static struct union "
                          "enum typedef sizeof if else for while do switch case default break continue "
                          "return goto extern")}},
    {"cpp", {"//", words_of("int char bool float double void auto const static struct class namespace "
                            "template typename public private protected virtual if else for while do switch "
                            "case default break continue return new delete this using nullptr true false "
                            "enum union")}},
    {"java", {"//", words_of("public private protected class interface extends implements static final void "
                             "int long double boolean char new return if else for while do switch case "
                             "default break continue this super import package try catch finally throw "
                             "throws null true false")}},
    {"go", {"//", words_of("func package import var const type struct interface map chan go defer if else "
                           "for range switch case default break continue return nil true false new make")}},
    {"rust", {"//", words_of("fn let mut const static struct enum impl trait pub use mod match if else for "
                             "while loop return self where as ref move dyn true false None Some Ok Err")}},
    {"typescript", {"//", words_of("function const let var interface type class enum public private protected "
                                   "readonly extends implements import export from if else for while return "
                                   "new this async await try catch throw typeof of as null undefined true "
                                   "false")}},
  };

  return known;
}

// An info string may carry more than the language, so only the first word of it
// selects a highlighter.
std::string first_word(std::string_view info) {
  const std::vector<std::string_view> words = text::words(info);

  return words.empty() ? std::string{} : text::to_lower_ascii(words.front());
}

void escape(std::string& out, std::string_view text) {
  for (const char character : text) {
    switch (character) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += character; break;
    }
  }
}

void span(std::string& out, std::string_view kind, std::string_view body) {
  out += "<span class=\"hl-";
  out += kind;
  out += "\">";
  escape(out, body);
  out += "</span>";
}

bool is_word_character(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '_';
}

bool is_digit(char character) {
  return character >= '0' && character <= '9';
}

}  // namespace

std::vector<std::string_view> languages() {
  std::vector<std::string_view> out;

  for (const auto& entry : languages_by_name()) {
    out.push_back(entry.first);
  }

  std::sort(out.begin(), out.end());

  return out;
}

bool supports(std::string_view language) {
  return languages_by_name().contains(first_word(language));
}

std::string render(std::string_view code, std::string_view language) {
  const auto found = languages_by_name().find(first_word(language));

  std::string out;
  out.reserve(code.size() * 2);

  if (found == languages_by_name().end()) {
    escape(out, code);

    return out;
  }

  const Language& definition = found->second;

  std::size_t position = 0;

  while (position < code.size()) {
    const std::string_view rest = code.substr(position);
    const char character = rest.front();

    if (!definition.comment.empty() && rest.starts_with(definition.comment)) {
      const auto newline = rest.find('\n');
      const std::string_view token = newline == std::string_view::npos ? rest : rest.substr(0, newline);

      span(out, "comment", token);
      position += token.size();
      continue;
    }

    if (character == '"' || character == '\'') {
      std::size_t scan = 1;

      while (scan < rest.size() && rest[scan] != character) {
        scan += rest[scan] == '\\' && scan + 1 < rest.size() ? 2UZ : 1UZ;
      }

      if (scan < rest.size()) {
        span(out, "string", rest.substr(0, scan + 1));
        position += scan + 1;
        continue;
      }
    }

    if (is_digit(character)) {
      std::size_t scan = 0;

      while (scan < rest.size() && (is_digit(rest[scan]) || rest[scan] == '.')) {
        ++scan;
      }

      span(out, "number", rest.substr(0, scan));
      position += scan;
      continue;
    }

    if (is_word_character(character) && !is_digit(character)) {
      std::size_t scan = 0;

      while (scan < rest.size() && is_word_character(rest[scan])) {
        ++scan;
      }

      const std::string_view token = rest.substr(0, scan);

      if (definition.keywords.contains(token)) {
        span(out, "keyword", token);
      } else {
        escape(out, token);
      }

      position += scan;
      continue;
    }

    escape(out, rest.substr(0, 1));
    ++position;
  }

  return out;
}

}  // namespace blogin::highlight
