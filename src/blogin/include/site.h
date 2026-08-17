#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "config.h"
#include "json.h"

namespace blogin {

struct BuildOptions {
  std::filesystem::path content;
  std::filesystem::path output;
  std::filesystem::path layouts;
  std::filesystem::path static_files;
  std::filesystem::path assets;
  std::filesystem::path data;
  std::filesystem::path shortcodes;

  // A theme's own layouts, static files, and assets. Each sits behind the
  // site's: a layout the site defines wins, and a file the site puts at the
  // same path wins. Empty when the site names no theme.
  std::filesystem::path theme_layouts;
  std::filesystem::path theme_static;
  std::filesystem::path theme_assets;

  // Prefixed onto every url the build produces, so a language's
  // pages under /<code>/ without any layout knowing about it.
  std::string url_prefix;

  // The languages the site is published in, and which one this build is. Empty
  // for a site with one language, which is most of them.
  std::vector<std::string> languages;
  std::string current_language;

  // Each language's translation keys and the url path each produces, so a page
  // can link to itself in another language, not to that language's home.
  std::map<std::string, std::map<std::string, std::string>> translations;

  Config config;

  bool drafts = false;
  bool future = false;
  bool force = false;

  // Zero means one worker per core.
  int jobs = 0;

  // Fills in the directories a site keeps beside its content, so a caller only
  // has to name the content directory.
  static BuildOptions around(const std::filesystem::path& content, Config config);
};

struct BuildReport {
  std::size_t pages = 0;

  // What the build did, as opposed to what it produced. A rebuild that
  // changed nothing should parse and render nothing, and these are what say so
  // without measuring time.
  std::size_t parsed = 0;
  std::size_t rendered = 0;

  std::size_t listings = 0;
  std::size_t written = 0;
  std::size_t skipped = 0;
  std::size_t fragments_reused = 0;

  // Fragment bodies rendered. Against the page count it says whether the
  // site's chrome was built once or once for every page carrying
  // it.
  std::size_t fragments_rendered = 0;

  std::vector<std::filesystem::path> changed;

  // Assets carried over from the last build without being read. An unchanged
  // image should cost a stat, not a read and a hash.
  std::size_t assets_reused = 0;

  // Things the build carried on without, which the caller should say out loud.
  std::vector<std::string> warnings;
};

std::expected<BuildReport, ParseError> build(const BuildOptions& options);

// Builds the whole site, which for most sites is one call to build.
//
// A site with languages configured is built once per language, each into its
// own /<code>/ subtree, sharing the layouts, assets, static files, data, and
// shortcodes at the site root. The root gets a page that redirects to the first
// language, since there is nothing else for / to be.
std::expected<BuildReport, ParseError> build_site(const BuildOptions& options);

// Every translation key in a content tree and the url path it produces. Read
// for each language before any of them is built, so a switcher can point at a
// translation that has not been built yet.
std::map<std::string, std::string> translation_paths(const BuildOptions& options);

// Removes the output tree. Refuses a target that is not inside the root, so a
// mistyped output directory cannot take something else with it.
std::expected<std::size_t, ParseError> clean(const std::filesystem::path& output,
                                             const std::filesystem::path& root);

}  // namespace blogin
