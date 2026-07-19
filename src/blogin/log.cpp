#include "log.h"

#include <iostream>
#include <mutex>

namespace blogin {
namespace {

// The preview server writes from a thread per connection, so two lines can be
// produced at once. One lock for the whole program keeps a line whole, and
// nothing here is on a hot path.
std::mutex& writing() {
  static std::mutex mutex;

  return mutex;
}

}  // namespace

LogLevel Log::level_from_name(std::string_view name) {
  if (name == "quiet") {
    return LogLevel::quiet;
  }

  if (name == "verbose") {
    return LogLevel::verbose;
  }

  return LogLevel::normal;
}

std::ostream& Log::out() const {
  return out_ != nullptr ? *out_ : std::cout;
}

std::ostream& Log::err() const {
  return err_ != nullptr ? *err_ : std::cerr;
}

bool Log::at_least(LogLevel level) const {
  return static_cast<int>(level_) >= static_cast<int>(level);
}

void Log::info(std::string_view message) const {
  const std::scoped_lock guard(writing());

  if (at_least(LogLevel::normal)) {
    out() << message << "\n";
  }
}

void Log::step(std::string_view message) const {
  const std::scoped_lock guard(writing());

  if (at_least(LogLevel::normal)) {
    out() << message << std::flush;
  }
}

void Log::verbose(std::string_view message) const {
  const std::scoped_lock guard(writing());

  if (at_least(LogLevel::verbose)) {
    out() << message << "\n";
  }
}

void Log::warn(std::string_view message) const {
  const std::scoped_lock guard(writing());

  if (at_least(LogLevel::normal)) {
    err() << message << "\n";
  }
}

void Log::error(std::string_view message) const {
  const std::scoped_lock guard(writing());

  err() << message << "\n";
}

}  // namespace blogin
