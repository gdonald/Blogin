#pragma once

#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace spec {

std::string describe_value(const std::string& value);
std::string describe_value(std::string_view value);
std::string describe_value(const char* value);
std::string describe_value(bool value);
std::string describe_value(std::nullptr_t);

template <typename Value>
  requires std::is_arithmetic_v<Value>
std::string describe_value(Value value) {
  return std::to_string(value);
}

template <typename Value>
std::string describe_value(const std::vector<Value>& values) {
  std::string out = "[";

  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out += ", ";
    }

    out += describe_value(values[index]);
  }

  return out + "]";
}

// Records a failure against the running example. Inside aggregate_failures the
// example keeps going and reports every failure at the end. Outside it, the
// first failure ends the example.
void record_failure(std::string message, std::source_location where);

void aggregate_failures(const std::function<void()>& body);

template <typename Actual>
class Expectation {
public:
  Expectation(Actual actual, std::source_location where)
    : actual_(std::move(actual)), where_(where) {}

  template <typename Expected>
  void to_eq(const Expected& expected) const {
    if (actual_ == expected) {
      return;
    }

    record_failure("expected: " + describe_value(expected) + "\n       got: " + describe_value(actual_), where_);
  }

  template <typename Expected>
  void not_to_eq(const Expected& expected) const {
    if (!(actual_ == expected)) {
      return;
    }

    record_failure("expected anything but: " + describe_value(expected), where_);
  }

  void to_contain(std::string_view needle) const {
    if (std::string_view(actual_).contains(needle)) {
      return;
    }

    record_failure("expected to contain: " + describe_value(needle) + "\n              in: " + describe_value(actual_),
                   where_);
  }

  void not_to_contain(std::string_view needle) const {
    if (!std::string_view(actual_).contains(needle)) {
      return;
    }

    record_failure("expected not to contain: " + describe_value(needle) + "\n                  in: " +
                     describe_value(actual_),
                   where_);
  }

  void to_start_with(std::string_view prefix) const {
    if (std::string_view(actual_).starts_with(prefix)) {
      return;
    }

    record_failure("expected to start with: " + describe_value(prefix) + "\n                 got: " +
                     describe_value(actual_),
                   where_);
  }

  void to_be_true() const {
    if (static_cast<bool>(actual_)) {
      return;
    }

    record_failure("expected true", where_);
  }

  void to_be_false() const {
    if (!static_cast<bool>(actual_)) {
      return;
    }

    record_failure("expected false", where_);
  }

  template <typename Expected>
  void to_be_less_than(const Expected& threshold) const {
    if (actual_ < threshold) {
      return;
    }

    record_failure("expected less than: " + describe_value(threshold) + "\n               got: " +
                     describe_value(actual_),
                   where_);
  }

  template <typename Expected>
  void to_be_greater_than(const Expected& threshold) const {
    if (actual_ > threshold) {
      return;
    }

    record_failure("expected greater than: " + describe_value(threshold) + "\n                  got: " +
                     describe_value(actual_),
                   where_);
  }

private:
  Actual actual_;
  std::source_location where_;
};

template <typename Actual>
Expectation<std::decay_t<Actual>> expect(Actual&& actual,
                                         std::source_location where = std::source_location::current()) {
  return Expectation<std::decay_t<Actual>>(std::forward<Actual>(actual), where);
}

// Per-example memoized value. Copies share one cache entry, so an `it` body may
// capture a let by value and still see the same instance the before hooks saw.
// The value is built on first use and discarded when the example ends.
std::any* let_slot(const void* key);

template <typename Factory>
class Let {
public:
  using Value = std::invoke_result_t<Factory>;

  explicit Let(Factory factory) : factory_(std::make_shared<Factory>(std::move(factory))) {}

  const Value& operator()() const {
    std::any* cell = let_slot(factory_.get());

    if (!cell->has_value()) {
      *cell = std::any((*factory_)());
    }

    return *std::any_cast<Value>(cell);
  }

private:
  std::shared_ptr<Factory> factory_;
};

template <typename Factory>
Let<Factory> let(Factory factory) {
  return Let<Factory>(std::move(factory));
}

void describe(std::string name, const std::function<void()>& body);

void context(std::string name, const std::function<void()>& body);

// Marks the enclosing group, and everything nested inside it, as needing
// exclusive access to process-global state: the work counters, the shared
// scratch directory, the current working directory. Those examples run on one
// thread after the parallel-safe ones finish.
//
// Reach for this only for process-global state. An example that is serial
// because it shares a fixture with its neighbours should get its own fixture
// instead.
void serial();

void before_each(std::function<void()> hook);

void after_each(std::function<void()> hook);

void it(const std::string& name, std::function<void()> body);

// Registered but not run, and reported as pending rather than passing.
void xit(const std::string& name, std::function<void()> body);

// A directory no other example and no other run will use, removed when the run
// ends.
//
// Unique per process as well as per call: a run killed partway leaves its
// directories behind, and a later run that could land on one of them would be
// reading another run's output.
std::filesystem::path scratch_directory(std::string_view label);

// Abandons the running example and reports it as pending rather than as
// passing. For an example whose subject depends on something the machine may
// not have, where silently passing would claim coverage that never ran.
[[noreturn]] void pending(std::string_view reason);

struct FileRegistrar {
  explicit FileRegistrar(void (*body)());
};

int run(int argc, char** argv);

}  // namespace spec

#define SPEC_CONCAT_INNER(a, b) a##b
#define SPEC_CONCAT(a, b) SPEC_CONCAT_INNER(a, b)

// One per spec file. Its body registers groups and examples, and nothing runs
// until the runner walks the tree.
#define SPEC                                                          \
  static void SPEC_CONCAT(spec_file_body_, __LINE__)();               \
  static const spec::FileRegistrar SPEC_CONCAT(spec_file_registrar_,   \
                                               __LINE__)(SPEC_CONCAT(spec_file_body_, __LINE__)); \
  static void SPEC_CONCAT(spec_file_body_, __LINE__)()
