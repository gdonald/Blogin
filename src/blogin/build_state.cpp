#include "build_state.h"
#include "files.h"

#include <chrono>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>

#include "json.h"
#include "writer.h"

namespace blogin {

std::pair<std::uintmax_t, std::int64_t> file_stamp(const std::filesystem::path& path) {
  std::error_code error;

  const std::uintmax_t size = std::filesystem::file_size(path, error);

  if (error) {
    return {0, 0};
  }

  const auto written = std::filesystem::last_write_time(path, error);

  if (error) {
    return {size, 0};
  }

  return {size, written.time_since_epoch().count()};
}

BuildState BuildState::load(const std::filesystem::path& path) {
  BuildState state;

  const auto parsed = parse_json(files::read_file(path));

  if (!parsed || !parsed->is_object()) {
    return state;
  }

  state.fingerprint = std::string((*parsed)["fingerprint"].as_string());

  state.started = (*parsed)["started"].as_integer();

  const auto read_sources = [](const Value& from, std::unordered_map<std::string, SourceState>& into) {
    for (const auto& member : from.members()) {
      SourceState source;
      source.size = static_cast<std::uintmax_t>(member.second["size"].as_integer());
      source.modified = member.second["modified"].as_integer();
      source.hash = std::string(member.second["hash"].as_string());
      source.output = std::string(member.second["output"].as_string());
      source.metadata = member.second["metadata"];

      into.emplace(member.first, std::move(source));
    }
  };

  state.listings = (*parsed)["listings"].as_integer();

  for (const Value& entry : (*parsed)["derived"].items()) {
    state.derived.emplace_back(entry.as_string());
  }

  read_sources((*parsed)["sources"], state.sources);
  read_sources((*parsed)["copies"], state.copies);

  return state;
}

void BuildState::save(const std::filesystem::path& path) const {
  const auto write_sources = [](const std::unordered_map<std::string, SourceState>& from) {
    Value into = Value::object();

    for (const auto& entry : from) {
      Value source = Value::object();
      source.set("size", Value(static_cast<std::int64_t>(entry.second.size)));
      source.set("modified", Value(entry.second.modified));
      source.set("hash", Value(entry.second.hash));
      source.set("output", Value(entry.second.output));
      source.set("metadata", entry.second.metadata);

      into.set(entry.first, std::move(source));
    }

    return into;
  };

  Value document = Value::object();
  document.set("fingerprint", Value(fingerprint));
  document.set("started", Value(started));
  Value derived_value = Value::array();

  for (const std::string& entry : derived) {
    derived_value.push(Value(entry));
  }

  document.set("listings", Value(listings));
  document.set("derived", std::move(derived_value));
  document.set("sources", write_sources(sources));
  document.set("copies", write_sources(copies));

  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  const std::string text = to_json(document, JsonStyle::compact, true);

  output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string source_hash(const std::filesystem::path& path) {
  return content_hash(files::read_file(path));
}

std::int64_t stamp_now() {
  return static_cast<std::int64_t>(
    std::filesystem::file_time_type::clock::now().time_since_epoch().count());
}

}  // namespace blogin
