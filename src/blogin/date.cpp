#include "date.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>

namespace blogin {
namespace {

constexpr std::array<const char*, 12> month_names{
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
};

constexpr std::array<const char*, 7> weekday_names{
  "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday",
};

bool all_digits(std::string_view text) {
  if (text.empty()) {
    return false;
  }

  return std::ranges::all_of(text, [](const char character) { return character >= '0' && character <= '9'; });
}

std::optional<int> to_int(std::string_view text) {
  if (!all_digits(text)) {
    return std::nullopt;
  }

  int value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

  if (result.ec != std::errc{}) {
    return std::nullopt;
  }

  return value;
}

std::optional<Date> parse_ymd(std::string_view text) {
  if (text.size() < 10 || text[4] != '-' || text[7] != '-') {
    return std::nullopt;
  }

  const std::optional<int> year = to_int(text.substr(0, 4));
  const std::optional<int> month = to_int(text.substr(5, 2));
  const std::optional<int> day = to_int(text.substr(8, 2));

  if (!year || !month || !day) {
    return std::nullopt;
  }

  const Date date(*year, static_cast<unsigned>(*month), static_cast<unsigned>(*day));

  if (!date.valid()) {
    return std::nullopt;
  }

  return date;
}

}  // namespace

Date::Date(int year, unsigned month, unsigned day)
  : date_(std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day}),
    valid_(date_.ok()) {}

std::optional<Date> Date::parse(std::string_view text) {
  if (text.size() != 10) {
    return std::nullopt;
  }

  return parse_ymd(text);
}

std::optional<Date> Date::parse_prefix(std::string_view text) {
  return parse_ymd(text);
}

int Date::year() const {
  return static_cast<int>(date_.year());
}

unsigned Date::month() const {
  return static_cast<unsigned>(date_.month());
}

unsigned Date::day() const {
  return static_cast<unsigned>(date_.day());
}

unsigned Date::weekday() const {
  const std::chrono::weekday day{std::chrono::sys_days{date_}};

  return day.iso_encoding();
}

std::string Date::iso() const {
  if (!valid_) {
    return {};
  }

  return std::format("{:04}-{:02}-{:02}", year(), month(), day());
}

std::string Date::format(std::string_view pattern) const {
  if (!valid_) {
    return {};
  }

  std::string out;
  out.reserve(pattern.size() + 16);

  for (std::size_t index = 0; index < pattern.size(); ++index) {
    if (pattern[index] != '%' || index + 1 >= pattern.size()) {
      out += pattern[index];
      continue;
    }

    const char specifier = pattern[++index];

    switch (specifier) {
      case 'Y':
        out += std::format("{:04}", year());
        break;
      case 'm':
        out += std::format("{:02}", month());
        break;
      case 'd':
        out += std::format("{:02}", day());
        break;
      case 'e':
        out += std::format("{}", day());
        break;
      case 'B':
        out += month_names[month() - 1];
        break;
      case 'b':
        out += std::string_view(month_names[month() - 1]).substr(0, 3);
        break;
      case 'A':
        out += weekday_names[weekday() - 1];
        break;
      case 'a':
        out += std::string_view(weekday_names[weekday() - 1]).substr(0, 3);
        break;
      case '%':
        out += '%';
        break;
      default:
        out += '%';
        out += specifier;
        break;
    }
  }

  return out;
}

}  // namespace blogin
