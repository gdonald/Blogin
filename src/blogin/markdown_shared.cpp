#include "markdown_internal.h"

#include <unordered_map>

#include "text.h"

namespace blogin::markdown_detail {
namespace {

// The named entities a blog post reaches for. The full HTML5 set runs past two
// thousand names, and every numeric reference resolves regardless.
const std::unordered_map<std::string, const char*>& named_entities() {
  static const std::unordered_map<std::string, const char*> entities{
    {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"}, {"nbsp", "\xc2\xa0"},
    {"copy", "\xc2\xa9"}, {"reg", "\xc2\xae"}, {"trade", "\xe2\x84\xa2"}, {"hellip", "\xe2\x80\xa6"},
    {"mdash", "\xe2\x80\x94"}, {"ndash", "\xe2\x80\x93"}, {"lsquo", "\xe2\x80\x98"},
    {"rsquo", "\xe2\x80\x99"}, {"ldquo", "\xe2\x80\x9c"}, {"rdquo", "\xe2\x80\x9d"},
    {"laquo", "\xc2\xab"}, {"raquo", "\xc2\xbb"}, {"deg", "\xc2\xb0"}, {"plusmn", "\xc2\xb1"},
    {"times", "\xc3\x97"}, {"divide", "\xc3\xb7"}, {"frac12", "\xc2\xbd"}, {"micro", "\xc2\xb5"},
    {"para", "\xc2\xb6"}, {"sect", "\xc2\xa7"}, {"dagger", "\xe2\x80\xa0"}, {"bull", "\xe2\x80\xa2"},
    {"larr", "\xe2\x86\x90"}, {"rarr", "\xe2\x86\x92"}, {"harr", "\xe2\x86\x94"},
    {"auml", "\xc3\xa4"}, {"ouml", "\xc3\xb6"}, {"uuml", "\xc3\xbc"}, {"szlig", "\xc3\x9f"},
    {"eacute", "\xc3\xa9"}, {"egrave", "\xc3\xa8"}, {"agrave", "\xc3\xa0"}, {"ccedil", "\xc3\xa7"},
    {"euro", "\xe2\x82\xac"}, {"pound", "\xc2\xa3"}, {"yen", "\xc2\xa5"}, {"cent", "\xc2\xa2"},
    {"AElig", "\xc3\x86"}, {"Dagger", "\xe2\x80\xa1"}, {"Auml", "\xc3\x84"}, {"Ouml", "\xc3\x96"},
    {"Uuml", "\xc3\x9c"}, {"ge", "\xe2\x89\xa5"}, {"le", "\xe2\x89\xa4"}, {"ne", "\xe2\x89\xa0"},
  };

  return entities;
}

}  // namespace

bool is_space_or_tab(char character) {
  return character == ' ' || character == '\t';
}

bool is_digit(char character) {
  return character >= '0' && character <= '9';
}

bool is_letter(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

bool is_blank(std::string_view line) {
  return line.find_first_not_of(" \t") == std::string_view::npos;
}

bool is_whitespace(char character) {
  return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
         character == '\f' || character == '\v';
}

bool is_punctuation(char character) {
  return std::string_view("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~").contains(character);
}

std::string normalize_label(std::string_view label) {
  std::string out;
  bool pending_space = false;

  for (const char character : text::trim(label)) {
    if (is_whitespace(character)) {
      pending_space = true;
      continue;
    }

    if (pending_space && !out.empty()) {
      out += ' ';
    }

    pending_space = false;
    out += static_cast<char>(character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character);
  }

  return out;
}

void append_utf8(std::string& out, std::uint32_t code_point) {
  if (code_point == 0 || code_point > 0x10FFFF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
    code_point = 0xFFFD;
  }

  if (code_point < 0x80U) {
    out += static_cast<char>(code_point);
  } else if (code_point < 0x800U) {
    out += static_cast<char>(0xC0U | (code_point >> 6));
    out += static_cast<char>(0x80U | (code_point & 0x3FU));
  } else if (code_point < 0x10000U) {
    out += static_cast<char>(0xE0U | (code_point >> 12));
    out += static_cast<char>(0x80U | ((code_point >> 6) & 0x3FU));
    out += static_cast<char>(0x80U | (code_point & 0x3FU));
  } else {
    out += static_cast<char>(0xF0U | (code_point >> 18));
    out += static_cast<char>(0x80U | ((code_point >> 12) & 0x3FU));
    out += static_cast<char>(0x80U | ((code_point >> 6) & 0x3FU));
    out += static_cast<char>(0x80U | (code_point & 0x3FU));
  }
}

std::size_t decode_entity(std::string_view input, std::string& out) {
  if (input.size() < 3 || input[0] != '&') {
    return 0;
  }

  const auto semicolon = input.find(';', 1);

  if (semicolon == std::string_view::npos) {
    return 0;
  }

  // The longest named entity is 32 characters, so a later semicolon belongs to
  // something else.
  if (semicolon > 33) {
    return 0;
  }

  const std::string_view body = input.substr(1, semicolon - 1);

  if (body.empty()) {
    return 0;
  }

  if (body[0] == '#') {
    std::uint32_t code_point = 0;
    const bool hexadecimal = body.size() > 2 && (body[1] == 'x' || body[1] == 'X');
    const std::string_view digits = body.substr(hexadecimal ? 2 : 1);

    if (digits.empty() || digits.size() > 8) {
      return 0;
    }

    for (const char character : digits) {
      std::uint32_t digit = 0;

      if (is_digit(character)) {
        digit = static_cast<std::uint32_t>(character - '0');
      } else if (hexadecimal && character >= 'a' && character <= 'f') {
        digit = static_cast<std::uint32_t>(character - 'a' + 10);
      } else if (hexadecimal && character >= 'A' && character <= 'F') {
        digit = static_cast<std::uint32_t>(character - 'A' + 10);
      } else {
        return 0;
      }

      code_point = (code_point * (hexadecimal ? 16U : 10U)) + digit;
    }

    append_utf8(out, code_point);

    return semicolon + 1;
  }

  const auto& entities = named_entities();
  const auto found = entities.find(std::string(body));

  if (found == entities.end()) {
    return 0;
  }

  out += found->second;

  return semicolon + 1;
}

std::uint32_t code_point_at(std::string_view text, std::size_t offset) {
  if (offset >= text.size()) {
    return 0;
  }

  const auto lead = static_cast<unsigned char>(text[offset]);

  if (lead < 0x80U) {
    return lead;
  }

  std::size_t length = 1;
  std::uint32_t value = 0;

  if ((lead & 0xE0U) == 0xC0U) {
    length = 2;
    value = lead & 0x1FU;
  } else if ((lead & 0xF0U) == 0xE0U) {
    length = 3;
    value = lead & 0x0FU;
  } else if ((lead & 0xF8U) == 0xF0U) {
    length = 4;
    value = lead & 0x07U;
  } else {
    return lead;
  }

  if (offset + length > text.size()) {
    return lead;
  }

  for (std::size_t index = 1; index < length; ++index) {
    const auto continuation = static_cast<unsigned char>(text[offset + index]);

    if ((continuation & 0xC0U) != 0x80U) {
      return lead;
    }

    value = (value << 6) | (continuation & 0x3FU);
  }

  return value;
}

std::uint32_t code_point_before(std::string_view text, std::size_t offset) {
  if (offset == 0) {
    return 0;
  }

  std::size_t start = offset - 1;

  while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) {
    --start;
  }

  return code_point_at(text, start);
}

bool is_unicode_whitespace(std::uint32_t code_point) {
  switch (code_point) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
    case 0x00A0:
    case 0x1680:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
      return true;
    default:
      return code_point >= 0x2000 && code_point <= 0x200A;
  }
}

// The punctuation and symbol ranges that appear in prose. The full Unicode
// category tables are not carried. What is here covers Latin-1, general
// punctuation, currency, arrows, mathematical operators, CJK punctuation, and
// fullwidth forms.
bool is_unicode_punctuation(std::uint32_t code_point) {
  if (code_point < 0x80) {
    return is_punctuation(static_cast<char>(code_point));
  }

  return (code_point >= 0x00A1 && code_point <= 0x00BF) || code_point == 0x00D7 || code_point == 0x00F7 ||
         (code_point >= 0x2010 && code_point <= 0x2027) || (code_point >= 0x2030 && code_point <= 0x205E) ||
         (code_point >= 0x20A0 && code_point <= 0x20BF) || (code_point >= 0x2100 && code_point <= 0x2BFF) ||
         (code_point >= 0x3001 && code_point <= 0x303F) || (code_point >= 0xFE30 && code_point <= 0xFE4F) ||
         (code_point >= 0xFF01 && code_point <= 0xFF65);
}

std::string unescape_string(std::string_view input) {
  std::string out;
  out.reserve(input.size());

  for (std::size_t index = 0; index < input.size(); ++index) {
    if (input[index] == '\\' && index + 1 < input.size() && is_punctuation(input[index + 1])) {
      out += input[++index];
      continue;
    }

    if (input[index] == '&') {
      if (const std::size_t consumed = decode_entity(input.substr(index), out); consumed > 0) {
        index += consumed - 1;
        continue;
      }
    }

    out += input[index];
  }

  return out;
}

}  // namespace blogin::markdown_detail
