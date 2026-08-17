#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace blogin {

// A calendar date, on std::chrono. Leap
// years and weekday calculation come from the standard library.
class Date {
public:
  Date() = default;

  Date(int year, unsigned month, unsigned day);

  // Parses exactly YYYY-MM-DD. A date that does not exist, such as 2023-02-30,
  // is rejected, never rolled forward.
  static std::optional<Date> parse(std::string_view text);

  // The leading YYYY-MM-DD of a filename, where a post's date comes
  // from when its front matter does not carry one.
  static std::optional<Date> parse_prefix(std::string_view text);

  bool valid() const { return valid_; }

  int year() const;
  unsigned month() const;
  unsigned day() const;

  // Monday is 1, Sunday is 7.
  unsigned weekday() const;

  std::string iso() const;

  // A strftime subset: %Y %m %d %e %B %b %A %a %%. An unknown specifier is
  // copied through with its percent sign, so a typo shows up in the output
  // instead of vanishing.
  std::string format(std::string_view pattern) const;

  friend bool operator==(const Date& left, const Date& right) = default;
  friend auto operator<=>(const Date& left, const Date& right) = default;

private:
  std::chrono::year_month_day date_{};
  bool valid_ = false;
};

}  // namespace blogin
