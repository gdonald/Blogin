#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "value.h"

namespace blogin {

// What a template is allowed to ask for.
//
// A layout can only read what the view offers, which keeps the
// expression language small: there is no host object graph to wander into. A
// name the view does not know is an error, never null, so a typo in a
// layout is visible instead of silently rendering nothing.
class ViewContext {
public:
  struct Argument {
    std::string name;
    Value value;
  };

  using Function = std::function<Value(const std::vector<Argument>&)>;

  void set(std::string name, Value value);

  void define(std::string name, Function function);

  // Locals shadow view names, so a loop variable takes precedence.
  void set_local(std::string name, Value value);

  bool has(std::string_view name) const;

  bool has_local(std::string_view name) const;

  bool callable(std::string_view name) const;

  const Value* lookup(std::string_view name) const;

  const Value* lookup_local(std::string_view name) const;

  const Function* function(std::string_view name) const;

  // A name close to one that exists, for "did you mean".
  std::string nearest(std::string_view name) const;

  // Locals come and go with a loop body or a partial's arguments, so they are
  // saved and restored around one.
  std::vector<std::pair<std::string, Value>> take_locals();

  void restore_locals(std::vector<std::pair<std::string, Value>> locals);

  // Recording what a fragment read lets its cache key be derived
  // than declared. A fragment that reads only site-level values produces the
  // same key on every page and is rendered once. One that reads page state
  // produces a different key per page and is rendered per page. Neither the
  // author nor the engine has to decide which it is.
  void begin_recording();

  // The names read since recording began, each with the value it had, as one
  // string. Two renders agree exactly when this does.
  std::string end_recording();

  bool recording() const { return !recordings_.empty(); }

  // The names the recording just ended read, in first-read order. Anything the
  // fragment bound itself is left out: a loop variable is decided by the
  // collection, which is recorded, so it is not a dependency of its own.
  const std::vector<std::string>& read_names() const { return read_names_; }

  // Whether replaying those names can stand in for rendering. A name that came
  // out as two different values within one render cannot, since resolving it
  // once now would speak for only one of them.
  bool replayable() const { return replayable_; }

  // The key those names produce against this page, without rendering anything.
  // Nothing when a name is no longer in scope, which sends the caller back to
  // rendering the fragment and recording it afresh.
  std::optional<std::string> replay(const std::vector<std::string>& names) const;

private:
  // One per fragment currently rendering. A fragment nested inside another
  // reads on behalf of both, so a read lands in every open recording.
  struct Recording {
    std::vector<std::pair<std::string, std::string>> reads;
    std::vector<std::string> bound;
    bool replayable = true;
  };

  void record(std::string_view name, const Value& value) const;

  void note_bound(std::string_view name);

  // Drops names the fragment bound and no longer has, so a read of the outer
  // value they were shadowing counts again.
  void prune_bound();

  const Value* resolve(std::string_view name) const;

  std::vector<std::pair<std::string, Value>> values_;
  std::vector<std::pair<std::string, Value>> locals_;
  std::vector<std::pair<std::string, Function>> functions_;

  mutable std::vector<Recording> recordings_;
  std::vector<std::string> read_names_;
  bool replayable_ = true;
};

}  // namespace blogin
