#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "config.h"

namespace blogin::reload {

// Where the preview page opens its socket. A path nothing else would ask for.
constexpr std::string_view socket_path = "/__blogin-reload";

std::string hello_message(std::int64_t version, std::string_view session);

std::string change_message(std::int64_t version, const std::vector<std::string>& paths);

// A failed rebuild is pushed too, so the error reaches the page rather than
// only the terminal the page's author is not looking at.
std::string failure_message(std::int64_t version, std::string_view detail, std::string_view file,
                            std::size_t line);

// The paths a build rewrote, relative to the output directory.
std::vector<std::string> changed_paths(const std::vector<std::filesystem::path>& files,
                                       const std::filesystem::path& root);

// The script the preview page runs, already wrapped in its script tag.
std::string client_script(std::int64_t version, std::string_view session);

// Puts the client before the closing body tag, or at the end of a page that has
// none.
std::string inject(std::string_view html, std::string_view script);

// Where a preview is built. Not the configured output directory, because a
// preview and a release disagree about what belongs in it: a preview is
// unminified and unfingerprinted, and it carries its own build state, so
// sharing one tree leaves whichever ran last in place. That is a deploy of
// unminified assets when the last writer was the server.
//
// Hidden, so it stays out of the way and the watcher already passes over it.
inline constexpr std::string_view preview_output_dir = ".blogin-preview";

// Fingerprinting and minification make production output cacheable, and both
// fight a preview: fingerprinting renames every asset on each rebuild, so a page
// already open points at files the rebuild just deleted, and there is no stable
// name left to swap a stylesheet under. A preview turns both off, which also
// takes back the time they cost on every save.
//
// It also builds somewhere else, so serving a site never writes the directory
// that gets deployed.
Config preview_config(Config config);

// Version, session, and the connected pages, shared by the socket threads.
class Channel {
public:
  Channel();

  // Called once per rebuild. Returns the message to push, and bumps the version
  // so a page that missed it can tell.
  std::string change(const std::vector<std::string>& paths);

  std::string failure(std::string_view detail, std::string_view file, std::size_t line);

  // What a page is told the moment it connects, so one that reloaded during a
  // rebuild can notice it is behind.
  std::string hello() const;

  std::int64_t version() const;

  const std::string& session() const { return session_; }

private:
  mutable std::mutex mutex_;
  std::int64_t version_ = 0;
  std::string session_;
};

}  // namespace blogin::reload
