#include "view_context.h"

#include <algorithm>

#include "json.h"
#include <format>
#include <utility>

namespace blogin {
namespace {

std::size_t edit_distance(std::string_view left, std::string_view right) {
  std::vector<std::size_t> previous(right.size() + 1);
  std::vector<std::size_t> current(right.size() + 1);

  for (std::size_t index = 0; index <= right.size(); ++index) {
    previous[index] = index;
  }

  for (std::size_t row = 1; row <= left.size(); ++row) {
    current[0] = row;

    for (std::size_t column = 1; column <= right.size(); ++column) {
      const std::size_t substitution = previous[column - 1] + (left[row - 1] == right[column - 1] ? 0 : 1);

      current[column] = std::min({current[column - 1] + 1, previous[column] + 1, substitution});
    }

    previous = current;
  }

  return previous[right.size()];
}

template <typename Entries>
auto find_entry(Entries& entries, std::string_view name) {
  return std::find_if(entries.begin(), entries.end(),
                      [&](const auto& entry) { return entry.first == name; });
}

// One read, written the one way, so a key built by replaying names matches the
// key the render that recorded them produced.
void append_read(std::string& key, std::string_view name, std::string_view value) {
  key += name;
  key += '=';
  key += value;
  key += ';';
}

}  // namespace

void ViewContext::set(std::string name, Value value) {
  if (const auto found = find_entry(values_, name); found != values_.end()) {
    found->second = std::move(value);

    return;
  }

  values_.emplace_back(std::move(name), std::move(value));
}

void ViewContext::define(std::string name, Function function) {
  if (const auto found = find_entry(functions_, name); found != functions_.end()) {
    found->second = std::move(function);

    return;
  }

  functions_.emplace_back(std::move(name), std::move(function));
}

void ViewContext::set_local(std::string name, Value value) {
  note_bound(name);

  if (const auto found = find_entry(locals_, name); found != locals_.end()) {
    found->second = std::move(value);

    return;
  }

  locals_.emplace_back(std::move(name), std::move(value));
}

bool ViewContext::has(std::string_view name) const {
  return lookup(name) != nullptr || callable(name);
}

bool ViewContext::has_local(std::string_view name) const {
  return lookup_local(name) != nullptr;
}

bool ViewContext::callable(std::string_view name) const {
  return function(name) != nullptr;
}

const Value* ViewContext::lookup(std::string_view name) const {
  const auto found = find_entry(values_, name);

  if (found == values_.end()) {
    return nullptr;
  }

  record(name, found->second);

  return &found->second;
}

const Value* ViewContext::lookup_local(std::string_view name) const {
  const auto found = find_entry(locals_, name);

  if (found == locals_.end()) {
    return nullptr;
  }

  record(name, found->second);

  return &found->second;
}

const ViewContext::Function* ViewContext::function(std::string_view name) const {
  const auto found = find_entry(functions_, name);

  return found == functions_.end() ? nullptr : &found->second;
}

std::string ViewContext::nearest(std::string_view name) const {
  std::string_view best;
  std::size_t best_distance = std::string_view::npos;

  const auto consider = [&](std::string_view candidate) {
    // Suggesting the name that was written helps nobody: if it matched, the
    // caller would not be asking.
    if (candidate == name) {
      return;
    }

    const std::size_t distance = edit_distance(name, candidate);

    if (distance < best_distance) {
      best_distance = distance;
      best = candidate;
    }
  };

  for (const auto& entry : values_) {
    consider(entry.first);
  }

  for (const auto& entry : functions_) {
    consider(entry.first);
  }

  // Far enough away and a suggestion is noise.
  if (best.empty() || best_distance > 3) {
    return {};
  }

  return std::format(" (did you mean '{}'?)", best);
}

void ViewContext::begin_recording() {
  recordings_.emplace_back();
}

std::string ViewContext::end_recording() {
  if (recordings_.empty()) {
    return {};
  }

  const Recording finished = std::move(recordings_.back());

  recordings_.pop_back();

  read_names_.clear();
  read_names_.reserve(finished.reads.size());

  std::string key;

  for (const auto& read : finished.reads) {
    read_names_.push_back(read.first);
    append_read(key, read.first, read.second);
  }

  replayable_ = finished.replayable;

  return key;
}

std::optional<std::string> ViewContext::replay(const std::vector<std::string>& names) const {
  std::string key;

  for (const std::string& name : names) {
    const Value* found = resolve(name);

    if (found == nullptr) {
      return std::nullopt;
    }

    append_read(key, name, to_json(*found, JsonStyle::compact, true));
  }

  return key;
}

const Value* ViewContext::resolve(std::string_view name) const {
  if (const Value* local = lookup_local(name)) {
    return local;
  }

  return lookup(name);
}

// A value is folded into the key by what it is, not by where it came from, so
// two pages that happen to read the same values share a fragment.
void ViewContext::record(std::string_view name, const Value& value) const {
  if (recordings_.empty()) {
    return;
  }

  const std::string text = to_json(value, JsonStyle::compact, true);

  for (Recording& recording : recordings_) {
    // A name the fragment bound itself is not a dependency of it: whatever
    // decided the binding was read, and that read is recorded.
    if (std::find(recording.bound.begin(), recording.bound.end(), name) != recording.bound.end()) {
      continue;
    }

    const auto seen = find_entry(recording.reads, name);

    if (seen == recording.reads.end()) {
      recording.reads.emplace_back(name, text);

      continue;
    }

    // One name, two values, in a single render. Resolving it once against the
    // next page would speak for only one of them, so this fragment is rendered
    // instead of replayed.
    if (seen->second != text) {
      recording.replayable = false;
    }
  }
}

void ViewContext::note_bound(std::string_view name) {
  for (Recording& recording : recordings_) {
    if (std::find(recording.bound.begin(), recording.bound.end(), name) == recording.bound.end()) {
      recording.bound.emplace_back(name);
    }
  }
}

void ViewContext::prune_bound() {
  for (Recording& recording : recordings_) {
    std::erase_if(recording.bound, [this](const std::string& name) {
      return find_entry(locals_, name) == locals_.end();
    });
  }
}

std::vector<std::pair<std::string, Value>> ViewContext::take_locals() {
  auto taken = std::exchange(locals_, {});

  prune_bound();

  return taken;
}

void ViewContext::restore_locals(std::vector<std::pair<std::string, Value>> locals) {
  locals_ = std::move(locals);

  prune_bound();
}

}  // namespace blogin
