#include "value.h"

#include <algorithm>

namespace blogin {
namespace {

const Value& null_value() {
  static const Value value;

  return value;
}

const Value::Array& empty_array() {
  static const Value::Array items;

  return items;
}

const Value::Object& empty_object() {
  static const Value::Object members;

  return members;
}

}  // namespace

Value Value::array() {
  Value value;
  value.storage_ = Array{};

  return value;
}

Value Value::array(std::initializer_list<Value> items) {
  Value value;
  value.storage_ = Array(items);

  return value;
}

Value Value::object() {
  Value value;
  value.storage_ = Object{};

  return value;
}

Value::Type Value::type() const {
  return static_cast<Type>(storage_.index());
}

bool Value::as_boolean(bool fallback) const {
  const bool* value = std::get_if<bool>(&storage_);

  return value != nullptr ? *value : fallback;
}

std::int64_t Value::as_integer(std::int64_t fallback) const {
  if (const std::int64_t* value = std::get_if<std::int64_t>(&storage_)) {
    return *value;
  }

  if (const double* value = std::get_if<double>(&storage_)) {
    return static_cast<std::int64_t>(*value);
  }

  return fallback;
}

double Value::as_number(double fallback) const {
  if (const double* value = std::get_if<double>(&storage_)) {
    return *value;
  }

  if (const std::int64_t* value = std::get_if<std::int64_t>(&storage_)) {
    return static_cast<double>(*value);
  }

  return fallback;
}

std::string_view Value::as_string(std::string_view fallback) const {
  const std::string* value = std::get_if<std::string>(&storage_);

  return value != nullptr ? std::string_view(*value) : fallback;
}

bool Value::truthy() const {
  switch (type()) {
    case Type::null:
      return false;
    case Type::boolean:
      return std::get<bool>(storage_);
    case Type::integer:
      return std::get<std::int64_t>(storage_) != 0;
    case Type::number:
      return std::get<double>(storage_) != 0.0;
    case Type::string:
      return !std::get<std::string>(storage_).empty();
    case Type::array:
      return !std::get<Array>(storage_).empty();
    case Type::object:
      return !std::get<Object>(storage_).empty();
  }

  return false;
}

std::size_t Value::size() const {
  if (const Array* items = std::get_if<Array>(&storage_)) {
    return items->size();
  }

  if (const Object* members = std::get_if<Object>(&storage_)) {
    return members->size();
  }

  if (const std::string* text = std::get_if<std::string>(&storage_)) {
    return text->size();
  }

  return 0;
}

void Value::push(Value item) {
  if (!is_array()) {
    storage_ = Array{};
  }

  std::get<Array>(storage_).push_back(std::move(item));
}

const Value& Value::at(std::size_t index) const {
  const Array* items = std::get_if<Array>(&storage_);

  if (items == nullptr || index >= items->size()) {
    return null_value();
  }

  return (*items)[index];
}

const Value::Array& Value::items() const {
  const Array* array_storage = std::get_if<Array>(&storage_);

  return array_storage != nullptr ? *array_storage : empty_array();
}

void Value::set(std::string key, Value item) {
  if (!is_object()) {
    storage_ = Object{};
  }

  auto& members = std::get<Object>(storage_);

  const auto found = std::find_if(members.begin(), members.end(),
                                  [&](const Member& member) { return member.first == key; });

  if (found != members.end()) {
    found->second = std::move(item);
    return;
  }

  members.emplace_back(std::move(key), std::move(item));
}

const Value* Value::find(std::string_view key) const {
  const Object* members = std::get_if<Object>(&storage_);

  if (members == nullptr) {
    return nullptr;
  }

  const auto found = std::find_if(members->begin(), members->end(),
                                  [&](const Member& member) { return member.first == key; });

  return found == members->end() ? nullptr : &found->second;
}

bool Value::contains(std::string_view key) const {
  return find(key) != nullptr;
}

const Value& Value::operator[](std::string_view key) const {
  const Value* found = find(key);

  return found != nullptr ? *found : null_value();
}

const Value::Object& Value::members() const {
  const Object* object_storage = std::get_if<Object>(&storage_);

  return object_storage != nullptr ? *object_storage : empty_object();
}

Value Value::deep_merge(const Value& base, const Value& over) {
  if (!base.is_object() || !over.is_object()) {
    return over;
  }

  Value merged = base;

  for (const Member& member : over.members()) {
    const Value* existing = merged.find(member.first);

    if (existing != nullptr && existing->is_object() && member.second.is_object()) {
      merged.set(member.first, deep_merge(*existing, member.second));
      continue;
    }

    merged.set(member.first, member.second);
  }

  return merged;
}

bool operator==(const Value& left, const Value& right) {
  return left.storage_ == right.storage_;
}

std::string_view type_name(Value::Type type) {
  switch (type) {
    case Value::Type::null:
      return "null";
    case Value::Type::boolean:
      return "boolean";
    case Value::Type::integer:
      return "integer";
    case Value::Type::number:
      return "number";
    case Value::Type::string:
      return "string";
    case Value::Type::array:
      return "array";
    case Value::Type::object:
      return "object";
  }

  return "unknown";
}

std::string Value::type_name() const {
  return std::string(blogin::type_name(type()));
}

}  // namespace blogin
