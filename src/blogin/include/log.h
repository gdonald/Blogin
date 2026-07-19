#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

namespace blogin {

enum class LogLevel {
  quiet,
  normal,
  verbose,
};

// Streams are injected so a spec can read what was written instead of watching
// the console.
class Log {
public:
  Log() = default;

  Log(LogLevel level, std::ostream& out, std::ostream& err) : level_(level), out_(&out), err_(&err) {}

  explicit Log(LogLevel level) : level_(level) {}

  static LogLevel level_from_name(std::string_view name);

  LogLevel level() const { return level_; }

  void info(std::string_view message) const;

  // Writes without a trailing newline, for a "label... " prefix that a later
  // call completes with a timing.
  void step(std::string_view message) const;

  void verbose(std::string_view message) const;

  void warn(std::string_view message) const;

  // Always written, whatever the level. An error the user cannot see is worse
  // than noise.
  void error(std::string_view message) const;

private:
  std::ostream& out() const;
  std::ostream& err() const;

  bool at_least(LogLevel level) const;

  LogLevel level_ = LogLevel::normal;
  std::ostream* out_ = nullptr;
  std::ostream* err_ = nullptr;
};

}  // namespace blogin
