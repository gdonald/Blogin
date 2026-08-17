#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace blogin {

// The dynamic type shared by JSON, YAML, data trees, and the template context.
//
// Objects keep insertion order. Configuration and data files are read and
// written back, and golden files compare bytes, so a map that reordered keys
// would turn every write into a spurious diff.
class Value {
public:
  enum class Type {
    null,
    boolean,
    integer,
    number,
    string,
    array,
    object,
  };

  using Array = std::vector<Value>;
  using Member = std::pair<std::string, Value>;
  using Object = std::vector<Member>;

  Value() = default;

  Value(bool value) : storage_(value) {}
  Value(std::int64_t value) : storage_(value) {}
  Value(int value) : storage_(static_cast<std::int64_t>(value)) {}
  Value(double value) : storage_(value) {}
  Value(std::string value) : storage_(std::move(value)) {}
  Value(const char* value) : storage_(std::string(value)) {}
  Value(std::string_view value) : storage_(std::string(value)) {}

  static Value array();
  static Value array(std::initializer_list<Value> items);
  static Value object();

  Type type() const;

  bool is_null() const { return type() == Type::null; }
  bool is_boolean() const { return type() == Type::boolean; }
  bool is_integer() const { return type() == Type::integer; }
  bool is_number() const { return type() == Type::number; }
  bool is_string() const { return type() == Type::string; }
  bool is_array() const { return type() == Type::array; }
  bool is_object() const { return type() == Type::object; }

  // A value read as the wrong type yields the fallback, never throwing.
  // Callers that care about the difference ask the type first. Callers reading
  // configuration with a default do not want to.
  bool as_boolean(bool fallback = false) const;
  std::int64_t as_integer(std::int64_t fallback = 0) const;
  double as_number(double fallback = 0.0) const;
  std::string_view as_string(std::string_view fallback = {}) const;

  // Null, false, zero, an empty string, and an empty collection are all false.
  bool truthy() const;

  std::size_t size() const;
  bool empty() const { return size() == 0; }

  void push(Value item);

  const Value& at(std::size_t index) const;

  const Array& items() const;

  void set(std::string key, Value item);

  bool contains(std::string_view key) const;

  const Value* find(std::string_view key) const;

  // Missing keys read as null, so a chain of lookups on absent data does not
  // need a check at every step.
  const Value& operator[](std::string_view key) const;

  const Object& members() const;

  // Nested objects merge recursively, and anything else in `over` replaces what
  // is in `base`. Keys new to `over` are appended in their own order.
  static Value deep_merge(const Value& base, const Value& over);

  friend bool operator==(const Value& left, const Value& right);

  std::string type_name() const;

private:
  std::variant<std::monostate, bool, std::int64_t, double, std::string, Array, Object> storage_;
};

std::string_view type_name(Value::Type type);

}  // namespace blogin
