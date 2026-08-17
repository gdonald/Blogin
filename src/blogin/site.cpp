#include "site.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <ios>
#include <iterator>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <thread>
#include <unordered_map>

#include "arena.h"
#include "assets.h"
#include "build_state.h"
#include "counters.h"
#include "data.h"
#include "feed.h"
#include "files.h"
#include "haml.h"
#include "html.h"
#include "markdown.h"
#include "metrics.h"
#include "nav.h"
#include "post.h"
#include "search.h"
#include "shortcode.h"
#include "slug.h"
#include "style.h"
#include "summary.h"
#include "template_store.h"
#include "text.h"
#include "view.h"
#include "writer.h"

namespace blogin {
namespace {

// One post on its way to becoming a page.
//
// Metadata lives here for the whole build: listings, feeds, and the search
// index all read it afterward. The body and its parse tree are released once
// the page is written, so peak memory tracks the number of posts.
struct Page {
  Post post;

  // False when the metadata came from the last build's state and the file was
  // never read.
  bool parsed = false;

  // True once the body has been read for what a listing needs from it: the
  // summary, the first image, and the word count. That happens before related
  // posts are worked out, because one post's related block quotes another
  // post's summary.
  bool prepared = false;
  std::string section;
  std::string url;
  std::string url_path;

  // The same name for this post in every language, so a switcher can link to
  // its translation.
  std::string translation_key;

  std::filesystem::path output;

  std::string summary;
  std::string first_image;
  std::size_t word_count = 0;
  int reading_time = 0;

  Value tags = Value::array();
  Value related = Value::array();

  // The posts either side of this one in its own section.
  std::string previous_url;
  std::string previous_title;
  std::string next_url;
  std::string next_title;
};

std::string join_url(std::string_view prefix, std::string_view path, bool clean_urls) {
  std::string url(prefix);
  url += '/';
  url += path;

  if (!clean_urls && !path.empty()) {
    url += '/';
  }

  return url;
}

// Where each language's version of this post lives, for a switcher.
//
// A language with no translation of it links to that language's home page.
Value language_switcher(const BuildOptions& options, std::string_view translation_key,
                        bool clean_urls) {
  Value out = Value::array();

  for (const std::string& code : options.languages) {
    std::string url = "/" + code + "/";

    if (const auto tree = options.translations.find(code); tree != options.translations.end()) {
      if (const auto found = tree->second.find(std::string(translation_key));
          found != tree->second.end()) {
        url = join_url("/" + code, found->second, clean_urls);
      }
    }

    Value entry = Value::object();
    entry.set("code", Value(code));
    entry.set("url", Value(url));
    entry.set("current", Value(code == options.current_language));

    out.push(std::move(entry));
  }

  return out;
}

// The same section in each language. A listing has no translation key, and its
// section name is the same everywhere anyway.
Value section_switcher(const BuildOptions& options, std::string_view section, bool clean_urls) {
  Value out = Value::array();

  for (const std::string& code : options.languages) {
    const std::string url =
      section.empty() ? "/" + code + "/" : join_url("/" + code, std::string(section), clean_urls);

    Value entry = Value::object();
    entry.set("code", Value(code));
    entry.set("url", Value(url));
    entry.set("current", Value(code == options.current_language));

    out.push(std::move(entry));
  }

  return out;
}

// A name for a post that is the same in every language: its section, plus its
// filename with the date and extension taken off.
//
// Not the slug: it follows the title, so it differs from language to language.
std::string translation_stem(const std::filesystem::path& file) {
  std::string stem = file.stem().string();

  // A leading date, with or without the separator after it.
  if (stem.size() >= 10 && stem[4] == '-' && stem[7] == '-') {
    bool dated = true;

    for (const std::size_t index : {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U}) {
      if (stem[index] < '0' || stem[index] > '9') {
        dated = false;

        break;
      }
    }

    if (dated) {
      stem.erase(0, stem.size() > 10 && stem[10] == '-' ? 11 : 10);
    }
  }

  return stem;
}

std::string section_of(const std::filesystem::path& file, const std::filesystem::path& content) {
  const std::string relative = std::filesystem::relative(file.parent_path(), content).generic_string();

  return relative == "." ? std::string{} : relative;
}

Value entry_of(const Page& page, std::string_view base = {}) {
  Value entry = Value::object();

  entry.set("title", Value(page.post.title));
  entry.set("url", Value(std::string(base) + page.url));
  entry.set("date", Value(page.post.date_string()));
  entry.set("summary", Value(page.summary));
  entry.set("description",
            Value(page.post.description.empty() ? page.summary : page.post.description));
  entry.set("image", Value(page.first_image));

  return entry;
}

// The first image in a rendered body, which a listing card shows.
std::string first_image_of(std::string_view html) {
  const auto tag = html.find("<img");

  if (tag == std::string_view::npos) {
    return {};
  }

  const auto source = html.find("src=\"", tag);

  if (source == std::string_view::npos) {
    return {};
  }

  const auto closing = html.find('"', source + 5);

  return closing == std::string_view::npos ? std::string{}
                                           : std::string(html.substr(source + 5, closing - source - 5));
}

bool newer_first(const Page& left, const Page& right) {
  const double left_order = left.post.order.value_or(1e18);
  const double right_order = right.post.order.value_or(1e18);

  if (left_order != right_order) {
    return left_order < right_order;
  }

  if (left.post.date != right.post.date) {
    return right.post.date < left.post.date;
  }

  return left.post.slug < right.post.slug;
}

// What a page's related block came out as, small enough to carry in the build
// state. The next build compares it against what it works out for that page,
// so a retitled post reaches the pages linking to it.
std::string related_key(const Value& related) {
  return content_hash(to_json(related));
}

// Related posts, by how many taxonomy terms two posts share.
//
// The term-to-posts index is built once and read back per post, so the work is
// one visit per term occurrence. Scoring every post against every other is 117k
// set intersections on the current corpus and 25M at 5,000 posts.
void assign_related(std::vector<Page>& pages, const Config& config) {
  std::vector<std::vector<std::string>> terms_of(pages.size());
  std::map<std::string, std::vector<std::size_t>> posts_by_term;

  for (std::size_t index = 0; index < pages.size(); ++index) {
    std::vector<std::string>& terms = terms_of[index];

    for (const std::string& taxonomy : config.taxonomies) {
      for (const std::string& term : pages[index].post.terms(taxonomy)) {
        terms.push_back(std::format("{}:{}", taxonomy, term));
      }
    }

    std::sort(terms.begin(), terms.end());
    terms.erase(std::unique(terms.begin(), terms.end()), terms.end());

    for (const std::string& term : terms) {
      posts_by_term[term].push_back(index);
    }
  }

  const std::size_t wanted =
    config.related_count > 0 ? static_cast<std::size_t>(config.related_count) : 0;

  // How many terms this post shares with each other post, kept across posts and
  // cleared through the list of the ones it touched, so a post with two tags
  // costs what its two tags cost.
  std::vector<std::size_t> shared(pages.size(), 0);
  std::vector<std::size_t> candidates;

  for (std::size_t index = 0; index < pages.size(); ++index) {
    Page& page = pages[index];

    page.related = Value::array();
    candidates.clear();

    for (const std::string& term : terms_of[index]) {
      count(Counter::related_terms);

      const auto listed = posts_by_term.find(term);

      if (listed == posts_by_term.end()) {
        continue;
      }

      for (const std::size_t other : listed->second) {
        if (other == index) {
          continue;
        }

        if (shared[other]++ == 0) {
          candidates.push_back(other);
        }
      }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](std::size_t left, std::size_t right) {
                       if (shared[left] != shared[right]) {
                         return shared[left] > shared[right];
                       }

                       return newer_first(pages[left], pages[right]);
                     });

    for (std::size_t position = 0; position < candidates.size() && position < wanted; ++position) {
      page.related.push(entry_of(pages[candidates[position]]));
    }

    for (const std::size_t other : candidates) {
      shared[other] = 0;
    }
  }
}

std::filesystem::path listing_file(const std::filesystem::path& output, std::string_view section,
                                   int page_number, bool at_root, bool clean_urls) {
  const std::string base = at_root ? std::string{} : std::string(section);

  if (page_number == 1) {
    if (base.empty()) {
      return output / "index.html";
    }

    return clean_urls ? output / (base + ".html") : output / base / "index.html";
  }

  const std::string relative =
    base.empty() ? std::format("page/{}", page_number) : std::format("{}/page/{}", base, page_number);

  return clean_urls ? output / (relative + ".html") : output / relative / "index.html";
}

std::string listing_url(std::string_view section, int page_number, bool at_root, bool clean_urls,
                        std::string_view prefix) {
  const std::string base = at_root ? std::string{} : std::string(section);

  const std::string path =
    page_number == 1 ? base
                     : (base.empty() ? std::format("page/{}", page_number)
                                     : std::format("{}/page/{}", base, page_number));

  if (path.empty()) {
    return prefix.empty() ? "/" : std::string(prefix) + "/";
  }

  return join_url(prefix, path, clean_urls);
}

// What a page has to remember so the next build can describe it without
// reading it: everything listings, feeds, and the search index ask for.
Value metadata_of(const Page& page) {
  Value tags = Value::array();

  for (const std::string& tag : page.post.tags) {
    tags.push(Value(tag));
  }

  Value meta = Value::object();

  for (const auto& entry : page.post.meta) {
    meta.set(entry.first, Value(entry.second));
  }

  Value out = Value::object();
  out.set("title", Value(page.post.title));
  out.set("date", Value(page.post.date_string()));
  out.set("slug", Value(page.post.slug));
  out.set("description", Value(page.post.description));
  out.set("summary", Value(page.summary));
  out.set("section", Value(page.section));
  out.set("url", Value(page.url));
  out.set("url-path", Value(page.url_path));
  out.set("translation-key", Value(page.translation_key));
  out.set("first-image", Value(page.first_image));
  out.set("word-count", Value(static_cast<std::int64_t>(page.word_count)));
  out.set("reading-time", Value(static_cast<std::int64_t>(page.reading_time)));
  out.set("tags", std::move(tags));
  out.set("meta", std::move(meta));

  // Recorded so the next build can tell whether the pages either side of this
  // one moved. They are what a post's own page links to, and neither of them
  // is derived from anything in the post's own file.
  out.set("prev-url", Value(page.previous_url));
  out.set("prev-title", Value(page.previous_title));
  out.set("next-url", Value(page.next_url));
  out.set("next-title", Value(page.next_title));

  // The same for the posts this one calls related, which are chosen from what
  // every other post says about itself.
  out.set("related-key", Value(related_key(page.related)));

  if (page.post.order.has_value()) {
    out.set("order", Value(*page.post.order));
  }

  return out;
}

void apply_metadata(Page& page, const Value& metadata) {
  page.post.title = std::string(metadata["title"].as_string());
  page.post.slug = std::string(metadata["slug"].as_string());
  page.post.description = std::string(metadata["description"].as_string());
  page.summary = std::string(metadata["summary"].as_string());
  page.section = std::string(metadata["section"].as_string());
  page.url = std::string(metadata["url"].as_string());
  page.url_path = std::string(metadata["url-path"].as_string());
  page.translation_key = std::string(metadata["translation-key"].as_string());
  page.first_image = std::string(metadata["first-image"].as_string());
  page.word_count = static_cast<std::size_t>(metadata["word-count"].as_integer());
  page.reading_time = static_cast<int>(metadata["reading-time"].as_integer());

  if (const std::optional<Date> date = Date::parse(metadata["date"].as_string())) {
    page.post.date = *date;
  }

  if (metadata.contains("order")) {
    page.post.order = metadata["order"].as_number();
  }

  for (const Value& tag : metadata["tags"].items()) {
    page.post.tags.emplace_back(tag.as_string());
  }

  for (const auto& entry : metadata["meta"].members()) {
    page.post.meta.emplace_back(entry.first, std::string(entry.second.as_string()));
  }

  Value tags = Value::array();

  for (const std::string& term : page.post.tags) {
    Value tag = Value::object();
    tag.set("name", Value(term));
    tag.set("url", Value(std::string{}));

    tags.push(std::move(tag));
  }

  page.tags = std::move(tags);
}

struct PlannedAsset {
  // Relative to the output directory, so it already carries any fingerprint.
  std::filesystem::path destination;
  std::string content;

  // Already on disk under this name from the last build, so the content was
  // never read. Hashing an unchanged image costs what copying it costs.
  bool reused = false;

  // Where it came from, kept for the case where the file turns out not to be on
  // disk after all and has to be written the ordinary way.
  std::filesystem::path source;
};

struct AssetPlan {
  std::vector<PlannedAsset> assets;

  // The url each asset was written under, for the ones whose name changed.
  std::map<std::string, std::string> manifest;

  // The sizes each image is available in, by its original url.
  std::map<std::string, std::string> srcsets;

  // Reported at the end of the build.
  std::vector<std::string> warnings;
};

// Resizes one image to each configured width smaller than it already is, and
// returns the bytes of each variant.
//
// The resizer is an external program that works on files, so each variant is
// made in a scratch directory, read, and removed. Everything the build writes
// goes through the writer, so pruning and the manifest account for all of it.
std::vector<std::pair<std::filesystem::path, std::string>> resize_variants(
  const std::filesystem::path& source, const std::filesystem::path& relative_path,
  const std::vector<int>& widths, std::string_view tool, int& source_width) {
  std::vector<std::pair<std::filesystem::path, std::string>> variants;

  source_width = assets::image_width(source, tool);

  if (source_width <= 0) {
    return variants;
  }

  std::vector<int> smaller;

  for (const int width : widths) {
    if (width < source_width) {
      smaller.push_back(width);
    }
  }

  std::sort(smaller.begin(), smaller.end());

  if (smaller.empty()) {
    return variants;
  }

  std::error_code error;

  // Named for this build and this image, so two builds resizing the same file
  // name at the same time cannot write over each other or delete each other's
  // work on the way out.
  static std::atomic<std::uint64_t> scratch_counter{0};

  const std::filesystem::path scratch =
    std::filesystem::temp_directory_path() /
    std::format("blogin-variants-{}-{}", stamp_now(), scratch_counter.fetch_add(1));

  std::filesystem::create_directories(scratch, error);

  for (const int width : smaller) {
    const std::string name = assets::variant_name(relative_path.filename().string(), width);
    const std::filesystem::path made = scratch / name;

    if (!assets::resize(source, made, width, tool)) {
      continue;
    }

    variants.emplace_back(relative_path.parent_path() / name, files::read_file(made));
    std::filesystem::remove(made, error);
  }

  std::filesystem::remove_all(scratch, error);

  return variants;
}

// Reads every asset, minifies what can be minified, and settles on the name each
// one will be written under.
//
// Stylesheets come last: one can reference an image, so its own bytes are not
// final until those references have been rewritten, and its name follows its
// bytes.
AssetPlan plan_assets(const BuildOptions& options, const Config& config,
                      const BuildState& previous_state, BuildState& state) {
  AssetPlan plan;

  struct Candidate {
    std::filesystem::path relative;
    std::string content;

    // Zero for anything the build made instead of reading, remembered
    // only for files that came off disk.
    std::uintmax_t size = 0;
    std::int64_t modified = 0;
    int width = 0;
    bool from_disk = false;
  };

  std::vector<Candidate> stylesheets;
  std::vector<Candidate> others;

  const auto minified = [&config](const std::filesystem::path& file, std::string content) {
    if (!config.minify) {
      return content;
    }

    const std::string extension = text::to_lower_ascii(file.extension().string());

    if (extension == ".css") {
      return assets::minify_css(content);
    }

    if (extension == ".js") {
      return assets::minify_js(content);
    }

    return content;
  };

  // Resizing is only attempted when the site asked for it and the machine has a
  // tool that can do it. A missing tool is reported, since the pages would
  // otherwise reference variants that were never written.
  const std::string tool = config.image_widths.empty() ? std::string{} : assets::resizer();

  if (!config.image_widths.empty() && tool.empty()) {
    plan.warnings.emplace_back("responsive images were requested but no image resizer (ImageMagick or sips) was found");
  }

  // Every url that turned into an asset, under the name it was written with
  // before any fingerprint. Declared here, not beside `place`, because a
  // reused image adds the variants it carries forward before anything is placed.
  std::set<std::string> placed;

  // Which original url each variant belongs to, so a srcset can be built once
  // the fingerprinted names are known.
  std::map<std::string, std::vector<assets::Variant>> variants_of;
  std::map<std::string, int> width_of;

  // A theme's assets and the site's own, by the path each is published under.
  // The theme's go in first so the site's overwrite them, one file at a time.
  std::map<std::filesystem::path, std::filesystem::path> asset_sources;

  if (!options.theme_assets.empty()) {
    for (const std::filesystem::path& file : files::all_files(options.theme_assets)) {
      asset_sources.insert_or_assign(std::filesystem::relative(file, options.theme_assets), file);
    }
  }

  for (const std::filesystem::path& file : files::all_files(options.assets)) {
    asset_sources.insert_or_assign(std::filesystem::relative(file, options.assets), file);
  }

  for (const auto& source : asset_sources) {
    const std::filesystem::path& relative_path = source.first;
    const std::filesystem::path& file = source.second;
    const std::string key = relative_path.generic_string();
    const auto [size, modified] = file_stamp(file);

    // A stylesheet is re-read whatever its stamp says, because its bytes depend
    // on the names of the images it references, not only on its own source.
    const bool stylesheet = text::to_lower_ascii(file.extension().string()) == ".css";
    const auto known = previous_state.copies.find(key);

    const bool stamped = !options.force && !stylesheet && known != previous_state.copies.end() &&
                         known->second.size == size && known->second.modified == modified;

    // An unchanged image is not resized again, so its variants are carried
    // forward. Unclaimed, the writer prunes them while the page naming them is
    // not re-rendered, leaving a srcset pointing at deleted files. Any missing
    // from the output tree means the image is treated as changed.
    std::vector<std::pair<std::string, std::string>> carried_variants;
    bool variants_on_disk = true;

    if (stamped) {
      const Value& recorded = known->second.metadata["variants"];

      for (std::size_t index = 0; index < recorded.size(); ++index) {
        const Value& variant = recorded.at(index);

        std::string variant_url(variant["url"].as_string());
        std::string variant_output(variant["output"].as_string());

        if (!std::filesystem::exists(options.output /
                                     std::filesystem::path(variant_output).lexically_relative("/"))) {
          variants_on_disk = false;
          break;
        }

        carried_variants.emplace_back(std::move(variant_url), std::move(variant_output));
      }
    }

    if (stamped && variants_on_disk &&
        (previous_state.settled(known->second) || source_hash(file) == known->second.hash)) {
      const std::string url = "/assets/" + key;

      if (known->second.output != url) {
        plan.manifest.emplace(url, known->second.output);
      }

      plan.assets.push_back(PlannedAsset{
        std::filesystem::path(known->second.output).lexically_relative("/"), {}, true, file});

      for (const auto& [variant_url, variant_output] : carried_variants) {
        placed.insert(variant_url);

        if (variant_output != variant_url) {
          plan.manifest.emplace(variant_url, variant_output);
        }

        plan.assets.push_back(PlannedAsset{
          std::filesystem::path(variant_output).lexically_relative("/"), {}, true, {}});
      }

      state.copies.emplace(key, known->second);

      if (known->second.metadata["width"].is_integer()) {
        width_of.emplace(url, static_cast<int>(known->second.metadata["width"].as_integer()));
      }

      continue;
    }

    Candidate candidate{relative_path, minified(file, files::read_file(file))};

    candidate.size = size;
    candidate.modified = modified;

    if (!tool.empty() && assets::is_raster(file)) {
      int source_width = 0;

      for (auto& made : resize_variants(file, relative_path, config.image_widths, tool, source_width)) {
        others.push_back(Candidate{made.first, std::move(made.second)});
      }

      if (source_width > 0) {
        candidate.width = source_width;
        width_of.emplace("/assets/" + relative_path.generic_string(), source_width);
      }
    }

    candidate.from_disk = true;

    if (text::to_lower_ascii(file.extension().string()) == ".css") {
      stylesheets.push_back(std::move(candidate));
    } else {
      others.push_back(std::move(candidate));
    }
  }

  // Generated alongside the site's own assets, and named the same way, so a
  // page cannot tell the difference.
  const auto generated = [&](std::string_view name, std::string content) {
    others.push_back(Candidate{std::filesystem::path(name), minified(name, std::move(content))});
  };

  if (config.search) {
    generated("js/search.js", search::script(config.search_cap));
  }

  generated("css/blogin.css", std::string(style::content_css()));

  if (config.search) {
    stylesheets.push_back(
      Candidate{"css/search.css", minified("css/search.css", std::string(search::stylesheet()))});
  }

  const auto place = [&plan, &config, &placed, &state](Candidate& candidate) {
    const std::filesystem::path destination = std::filesystem::path("assets") / candidate.relative;
    const std::string url = "/" + destination.generic_string();

    placed.insert(url);

    std::filesystem::path written = destination;

    if (config.fingerprint && assets::is_fingerprintable(candidate.relative)) {
      const std::string hash = content_hash(candidate.content);

      written = destination.parent_path() /
                assets::fingerprint_name(candidate.relative.filename().string(), hash);

      plan.manifest.emplace(url, "/" + written.generic_string());
    }

    // Only what came off disk can be recognised by its stamp next time. What the
    // build generated is regenerated either way.
    if (candidate.from_disk) {
      SourceState entry;
      entry.size = candidate.size;
      entry.modified = candidate.modified;
      entry.output = "/" + written.generic_string();

      if (candidate.modified >= state.started) {
        entry.hash = content_hash(candidate.content);
      }

      if (candidate.width > 0) {
        entry.metadata.set("width", Value(static_cast<std::int64_t>(candidate.width)));
      }

      state.copies.insert_or_assign(candidate.relative.generic_string(), std::move(entry));
    }

    plan.assets.push_back(PlannedAsset{written, std::move(candidate.content), false, {}});
  };

  for (Candidate& candidate : others) {
    place(candidate);
  }

  for (Candidate& candidate : stylesheets) {
    candidate.content = assets::rewrite_refs(candidate.content, plan.manifest);

    place(candidate);
  }

  // Built last, because a variant's url is only settled once it has been placed
  // and possibly renamed.
  for (const auto& original : width_of) {
    std::vector<assets::Variant> found;

    for (const int width : config.image_widths) {
      if (width >= original.second) {
        continue;
      }

      const std::filesystem::path relative_path =
        std::filesystem::path(original.first).lexically_relative("/assets");
      const std::string url =
        "/assets/" + (relative_path.parent_path() /
                      assets::variant_name(relative_path.filename().string(), width))
                       .generic_string();

      // A width the resizer could not produce is not offered.
      if (!placed.contains(url)) {
        continue;
      }

      found.push_back(assets::Variant{width, url});
    }

    if (found.empty()) {
      continue;
    }

    plan.srcsets.emplace(original.first,
                         assets::srcset_value(original.first, original.second, found));

    // Remembered against the image they came from, so the next build carries
    // them forward without resizing. Unrecorded, the writer prunes them while
    // the pages naming them stay as they are.
    const auto entry = state.copies.find(
      std::filesystem::path(original.first).lexically_relative("/assets").generic_string());

    if (entry == state.copies.end()) {
      continue;
    }

    Value recorded = Value::array();

    for (const assets::Variant& variant : found) {
      const auto renamed = plan.manifest.find(variant.url);

      Value one = Value::object();
      one.set("url", Value(variant.url));
      one.set("output", Value(renamed == plan.manifest.end() ? variant.url : renamed->second));

      recorded.push(std::move(one));
    }

    entry->second.metadata.set("variants", std::move(recorded));
  }

  return plan;
}

// Everything outside the content tree that could change any page. One value,
// and any difference rebuilds the lot, which is fast in the common case and
// correct in every case.
std::string global_fingerprint(const BuildOptions& options) {
  std::string material;

  // What a page is rendered from. A difference here can change any output, so
  // the bytes are what count.
  const auto add_file = [&](const std::filesystem::path& path) {
    material += path.generic_string();
    material += source_hash(path);
  };

  add_file(options.content.parent_path() / "blogin.json");

  for (const std::filesystem::path& directory :
       {options.layouts, options.theme_layouts, options.data, options.shortcodes}) {
    for (const std::filesystem::path& file : files::all_files(directory)) {
      add_file(file);
    }
  }

  // Static files and assets are absent on purpose. Nothing renders from them,
  // so a new image is not a reason to render every page again. They are copied
  // through, and the copy notices its own changes.

  // A data file beside the content counts too, since a section reads it.
  for (const std::filesystem::path& file : files::all_files(options.content)) {
    if (file.filename().string().starts_with("_data.")) {
      add_file(file);
    }
  }

  return content_hash(material);
}

}  // namespace

BuildOptions BuildOptions::around(const std::filesystem::path& content, Config config) {
  BuildOptions options;

  const std::filesystem::path root = content.parent_path();

  options.content = content;
  options.output = root / config.output_dir;
  options.layouts = root / "layouts";
  options.static_files = root / "static";
  options.assets = root / "assets";
  options.data = root / "data";
  options.shortcodes = root / "shortcodes";

  if (!config.theme.empty()) {
    const std::filesystem::path theme = root / "themes" / config.theme;

    options.theme_layouts = theme / "layouts";
    options.theme_static = theme / "static";
    options.theme_assets = theme / "assets";
  }

  options.config = std::move(config);

  return options;
}

namespace {

// A page whose only job is to send a browser somewhere else: the default
// language at the site root, and the canonical url for each alias a post lists.
std::string redirect_html(std::string_view target) {
  std::string escaped;
  append_attribute_escaped(escaped, target);

  return std::format(R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="refresh" content="0; url={0}">
<link rel="canonical" href="{0}">
<title>Redirecting</title>
</head>
<body>
<p>Redirecting to <a href="{0}">{0}</a>.</p>
</body>
</html>
)HTML",
                     escaped);
}

// What a site with no 404 layout of its own gets. A static host serves it for
// any url it cannot find.
std::string not_found_html() {
  return R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>404 Not Found</title>
</head>
<body>
<h1>404</h1>
<p>The page you are looking for was not found.</p>
<p><a href="/">Home</a></p>
</body>
</html>
)HTML";
}

// Where a url lands on disk, for the aliases a post lists.
std::filesystem::path url_to_file(const std::filesystem::path& output, std::string_view url,
                                  bool clean_urls) {
  std::string_view relative = url;

  while (relative.starts_with('/')) {
    relative.remove_prefix(1);
  }

  while (relative.ends_with('/')) {
    relative.remove_suffix(1);
  }

  if (relative.empty()) {
    return output / "index.html";
  }

  return clean_urls ? output / (std::string(relative) + ".html")
                    : output / relative / "index.html";
}

}  // namespace

std::map<std::string, std::string> translation_paths(const BuildOptions& options) {
  std::map<std::string, std::string> paths;

  const bool clean_urls = options.config.clean_urls;

  (void)clean_urls;

  for (const std::filesystem::path& file : files::files_with_extension(options.content, ".md")) {
    auto post = Post::load(file);

    if (!post) {
      continue;
    }

    if (post->draft && !options.drafts) {
      continue;
    }

    if (!options.future && post->date.valid()) {
      const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
      const std::chrono::year_month_day now{today};

      if (Date(static_cast<int>(now.year()), static_cast<unsigned>(now.month()),
               static_cast<unsigned>(now.day())) < post->date) {
        continue;
      }
    }

    const std::string section = section_of(file, options.content);
    const std::string stem = translation_stem(file);

    const std::string prefix = section.empty() ? std::string{} : section + "/";

    paths.emplace(prefix + stem, prefix + post->slug);
  }

  return paths;
}

std::expected<BuildReport, ParseError> build_site(const BuildOptions& options) {
  if (options.config.languages.empty()) {
    return build(options);
  }

  // Every language's paths are read before any of them is built, so a switcher
  // on the first language can point at the last one's pages.
  std::map<std::string, std::map<std::string, std::string>> translations;

  for (const std::string& code : options.config.languages) {
    BuildOptions scan = options;
    scan.content = options.content / code;

    translations.emplace(code, translation_paths(scan));
  }

  BuildReport total;

  for (const std::string& code : options.config.languages) {
    BuildOptions language = options;

    language.content = options.content / code;
    language.output = options.output / code;
    language.url_prefix = "/" + code;
    language.languages = options.config.languages;
    language.current_language = code;
    language.translations = translations;

    // A language may give the site a different name, and nothing else.
    if (const auto named = std::find_if(options.config.language_config.begin(),
                                        options.config.language_config.end(),
                                        [&code](const LanguageConfig& entry) {
                                          return entry.code == code;
                                        });
        named != options.config.language_config.end()) {
      language.config.title = named->title.value_or(language.config.title);
    }

    auto report = build(language);

    if (!report) {
      return std::unexpected(report.error());
    }

    total.pages += report->pages;
    total.parsed += report->parsed;
    total.rendered += report->rendered;
    total.listings += report->listings;
    total.written += report->written;
    total.skipped += report->skipped;
    total.fragments_reused += report->fragments_reused;
    total.fragments_rendered += report->fragments_rendered;
    total.assets_reused += report->assets_reused;

    for (const std::string& warning : report->warnings) {
      total.warnings.push_back(warning);
    }

    for (const std::filesystem::path& changed : report->changed) {
      total.changed.push_back(changed);
    }
  }

  std::error_code error;
  std::filesystem::create_directories(options.output, error);

  const std::filesystem::path index = options.output / "index.html";
  const std::string redirect = redirect_html("/" + options.config.languages.front() + "/");

  // Written directly, not through a writer: it belongs to no language's output
  // tree, and each language's writer prunes only its own.
  if (files::read_file(index) != redirect) {
    std::ofstream out(index, std::ios::binary | std::ios::trunc);

    out.write(redirect.data(), static_cast<std::streamsize>(redirect.size()));

    total.written += 1;
    total.changed.push_back(index);
  }

  return total;
}

std::expected<std::size_t, ParseError> clean(const std::filesystem::path& output,
                                             const std::filesystem::path& root) {
  if (!files::within(output, root)) {
    return std::unexpected(ParseError{
      std::format("refusing to clean '{}': it is not inside '{}'", output.string(), root.string()), 1, 1});
  }

  const std::vector<std::filesystem::path> found = files::all_files(output);

  std::size_t removed = 0;
  std::error_code error;

  for (const std::filesystem::path& path : found) {
    if (path.filename() == files::keep_file) {
      continue;
    }

    if (std::filesystem::remove(path, error)) {
      ++removed;
    }
  }

  files::prune_empty_directories(output);
  std::filesystem::remove(output, error);

  return removed;
}

std::expected<BuildReport, ParseError> build(const BuildOptions& options) {
  // Building where there is nothing to build wrote an empty site and reported
  // success, which reads as a site that lost its posts. An empty content
  // directory is a new site, and it builds.
  std::error_code missing;

  if (!std::filesystem::is_directory(options.content, missing)) {
    return std::unexpected(ParseError{
      std::format("no content directory at '{}'. Is this a Blogin site?", options.content.string()),
      1, 1});
  }

  const Config& config = options.config;
  const bool clean_urls = config.clean_urls;

  // The site's own layouts come first, so one of its templates shadows the
  // theme's template of the same name.
  std::vector<std::filesystem::path> layout_paths{options.layouts};

  if (!options.theme_layouts.empty()) {
    layout_paths.push_back(options.theme_layouts);
  }

  auto store = TemplateStore::load(layout_paths);

  if (!store) {
    return std::unexpected(store.error());
  }

  auto global_data = data::load_tree(options.data);

  if (!global_data) {
    return std::unexpected(global_data.error());
  }

  const ShortcodeRegistry shortcodes = ShortcodeRegistry::load(options.shortcodes);
  const Framework framework = Framework::profile(config.css_framework);
  const std::vector<NavNode> nav =
    nav::build_tree(options.content, config, {clean_urls, options.url_prefix});

  // One walk of the content tree, shared by everything that follows.
  const std::vector<std::filesystem::path> sources =
    files::files_with_extension(options.content, ".md");

  Writer writer(options.output, options.force);
  writer.load_manifest();

  const std::filesystem::path state_path = options.output / ".blogin-state.json";

  BuildState previous_state = BuildState::load(state_path);
  BuildState state;
  state.started = stamp_now();
  state.fingerprint = global_fingerprint(options);

  // Assets are planned before anything renders, because a page that references
  // one has to be written with the name that asset will have. Working
  // it out afterwards would mean rewriting finished pages on disk, leaving the
  // output dependent on the order things ran in.
  // What the static trees publish, and which file each path comes from. The
  // theme's go in first so the site's replace them, one file at a time. A file
  // shipped by hand also suppresses the one the build would generate.
  std::map<std::filesystem::path, std::filesystem::path> static_sources;

  for (const std::filesystem::path& tree : {options.theme_static, options.static_files}) {
    if (tree.empty()) {
      continue;
    }

    for (const std::filesystem::path& file : files::all_files(tree)) {
      static_sources.insert_or_assign(std::filesystem::relative(file, tree), file);
    }
  }

  std::set<std::string> shipped;

  for (const auto& source : static_sources) {
    shipped.insert(source.first.generic_string());
  }

  const AssetPlan plan = plan_assets(options, config, previous_state, state);

  // Anything the build wants said out loud that the asset plan did not raise.
  std::vector<std::string> warnings;

  for (const auto& entry : plan.manifest) {
    state.fingerprint += entry.first;
    state.fingerprint += entry.second;
  }

  state.fingerprint = content_hash(state.fingerprint);

  // Order matters: a page names images by the url they were written with, so the
  // srcset is added first and both it and the src are renamed afterwards.
  if (!plan.manifest.empty() || !plan.srcsets.empty()) {
    writer.set_page_filter([&plan](std::string_view page) -> std::string {
      return assets::rewrite_refs(assets::add_srcset(page, plan.srcsets), plan.manifest);
    });
  }

  // A change outside the content tree can affect any page, so the whole site is
  // rebuilt.
  const bool reusable = !options.force && !previous_state.fingerprint.empty() &&
                        previous_state.fingerprint == state.fingerprint;

  std::vector<Page> pages;
  std::vector<std::size_t> source_of_page;

  // Stamped once, before the file is read. Stamping again after rendering
  // would record a file edited mid-build as one this build already accounted
  // for, and that edit would never be seen again.
  std::vector<std::pair<std::uintmax_t, std::int64_t>> stamps(sources.size());
  std::unordered_map<std::string, std::string> seen;

  for (std::size_t source_index = 0; source_index < sources.size(); ++source_index) {
    const std::filesystem::path& file = sources[source_index];
    const std::string key = std::filesystem::relative(file, options.content).generic_string();
    const auto [size, modified] = file_stamp(file);
    stamps[source_index] = {size, modified};

    if (reusable) {
      const auto known = previous_state.sources.find(key);

      // Size and time answer this without opening the file. Reading it to hash
      // would be most of the cost of parsing it, so the bytes are only compared
      // for a file whose stamp fell inside the last build, where the stamp
      // cannot be believed.
      const bool stamped = known != previous_state.sources.end() && known->second.size == size &&
                           known->second.modified == modified;

      if (stamped && (previous_state.settled(known->second) ||
                      source_hash(file) == known->second.hash)) {
        Page page;
        apply_metadata(page, known->second.metadata);
        page.output = known->second.output;

        // Its page is already on disk, and whether it is still correct depends
        // on the posts either side of it, which are not known until every
        // source has been looked at. Kept below, once they are.
        if (writer.reusable(page.output)) {
          seen.emplace(page.output.generic_string(), file.string());

          state.sources.emplace(key, known->second);
          source_of_page.push_back(source_index);
          pages.push_back(std::move(page));

          continue;
        }
      }
    }

    auto post = Post::load(file);

    if (!post) {
      return std::unexpected(post.error());
    }

    if (post->draft && !options.drafts) {
      continue;
    }

    if (!options.future && post->date.valid()) {
      // A post dated ahead of today is held back until that day arrives.
      const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
      const std::chrono::year_month_day now{today};

      if (Date(static_cast<int>(now.year()), static_cast<unsigned>(now.month()),
               static_cast<unsigned>(now.day())) < post->date) {
        continue;
      }
    }

    Page page;
    page.parsed = true;
    page.section = section_of(file, options.content);
    page.post = std::move(*post);

    const bool is_root_index = page.section.empty() && file.stem() == "index";

    page.url_path = page.section.empty() ? page.post.slug : page.section + "/" + page.post.slug;
    page.url = is_root_index ? options.url_prefix + "/"
                             : join_url(options.url_prefix, page.url_path, clean_urls);
    page.output = is_root_index ? options.output / "index.html"
                                : (clean_urls ? options.output / (page.url_path + ".html")
                                              : options.output / page.url_path / "index.html");

    page.translation_key = page.section.empty()
                             ? translation_stem(file)
                             : page.section + "/" + translation_stem(file);

    const std::string page_key = page.output.generic_string();

    if (const auto clash = seen.find(page_key); clash != seen.end()) {
      return std::unexpected(ParseError{
        std::format("two posts write the same page '{}': {} and {}", page.url, clash->second,
                    file.string()),
        1, 1});
    }

    seen.emplace(page_key, file.string());
    source_of_page.push_back(source_index);
    pages.push_back(std::move(page));
  }

  // The posts either side of this one, in the order its section lists them.
  //
  // A post's neighbours are not a function of its own file, so adding,
  // removing, or retitling one changes the two pages around it. Both are
  // re-read and re-rendered here.
  {
    std::map<std::string, std::vector<std::size_t>> section_order;

    for (std::size_t index = 0; index < pages.size(); ++index) {
      section_order[pages[index].section].push_back(index);
    }

    for (auto& entry : section_order) {
      std::vector<std::size_t>& members = entry.second;

      std::stable_sort(members.begin(), members.end(), [&pages](std::size_t left, std::size_t right) {
        return newer_first(pages[left], pages[right]);
      });

      for (std::size_t position = 0; position < members.size(); ++position) {
        Page& page = pages[members[position]];

        if (position > 0) {
          const Page& earlier = pages[members[position - 1]];
          page.previous_url = earlier.url;
          page.previous_title = earlier.post.title;
        }

        if (position + 1 < members.size()) {
          const Page& later = pages[members[position + 1]];
          page.next_url = later.url;
          page.next_title = later.post.title;
        }
      }
    }

    for (std::size_t index = 0; index < pages.size(); ++index) {
      Page& page = pages[index];

      if (page.parsed) {
        continue;
      }

      const std::filesystem::path& file = sources[source_of_page[index]];
      const std::string key = std::filesystem::relative(file, options.content).generic_string();
      const auto known = previous_state.sources.find(key);

      bool settled = false;

      if (known != previous_state.sources.end()) {
        const Value& before = known->second.metadata;

        settled = before["prev-url"].as_string() == page.previous_url &&
                  before["prev-title"].as_string() == page.previous_title &&
                  before["next-url"].as_string() == page.next_url &&
                  before["next-title"].as_string() == page.next_title;
      }

      // Kept, not kept-and-committed: whether this page can be left
      // alone also depends on its related posts, which are not known until
      // every page's summary is.
      if (settled && writer.reusable(page.output)) {
        continue;
      }

      auto post = Post::load(file);

      if (!post) {
        return std::unexpected(post.error());
      }

      // Back to what reading the file the ordinary way produces, so this page
      // renders to the same bytes a from-scratch build would give it. The tags
      // restored from the state carry no urls and are rebuilt by the render.
      page.post = std::move(*post);
      page.tags = Value::array();
      page.parsed = true;
    }
  }

  FragmentCache fragments;

  const unsigned workers =
    options.jobs > 0 ? static_cast<unsigned>(options.jobs) : std::max(1U, std::thread::hardware_concurrency());

  RenderOptions render_options;
  render_options.framework = framework;
  render_options.highlight = config.highlight;
  render_options.shortcodes = &shortcodes;
  render_options.heading_anchors = true;

  // Runs a task over every page across the thread pool, stopping at the first
  // failure. A task reports success as a ParseError on line zero, so it is
  // returned from a worker instead of thrown across one.
  const auto over_pages = [&](const std::function<ParseError(std::size_t)>& task)
    -> std::expected<void, ParseError> {
    std::atomic<std::size_t> next{0};
    std::atomic<bool> failed{false};
    ParseError failure;
    std::mutex failure_mutex;

    const auto worker = [&] {
      for (;;) {
        const std::size_t index = next.fetch_add(1);

        if (index >= pages.size() || failed.load()) {
          return;
        }

        ParseError error = task(index);

        if (error.line != 0) {
          const std::scoped_lock guard(failure_mutex);

          if (!failed.exchange(true)) {
            failure = std::move(error);
          }

          return;
        }
      }
    };

    if (workers == 1 || pages.size() < 2) {
      worker();
    } else {
      std::vector<std::thread> threads;

      threads.reserve(workers);
      for (unsigned index = 0; index < workers; ++index) {
        threads.emplace_back(worker);
      }

      for (std::thread& thread : threads) {
        thread.join();
      }
    }

    if (failed.load()) {
      return std::unexpected(failure);
    }

    return {};
  };

  // What a listing, a feed, or another post's related block asks of this post's
  // body. It is read here, not during the render, because every page's
  // summary has to exist before any page's related posts can be chosen.
  const auto prepare_page = [&](std::size_t index) {
    Page& page = pages[index];

    if (!page.parsed || page.prepared) {
      return ParseError{{}, 0, 0};
    }

    page.prepared = true;

    Arena arena;

    const RenderResult body = render_document(parse_markdown(arena, page.post.body), render_options);

    page.word_count = metrics::word_count(body.text);
    page.reading_time = metrics::reading_time(page.word_count, config.reading_wpm);
    page.first_image = first_image_of(body.html);

    std::string excerpt;

    if (const auto marker = page.post.body.find(summary::more_marker); marker != std::string::npos) {
      Arena excerpt_arena;

      excerpt = render_document(parse_markdown(excerpt_arena, page.post.body.substr(0, marker)),
                                render_options)
                  .text;
    }

    page.summary = summary::choose(page.post.summary, excerpt, body.text,
                                   static_cast<std::size_t>(config.summary_length));

    page.tags = Value::array();

    for (const std::string& term : page.post.terms("tags")) {
      Value tag = Value::object();
      tag.set("name", Value(term));
      tag.set("url", Value(join_url({}, "tags/" + slug::slugify(term), clean_urls)));

      page.tags.push(std::move(tag));
    }

    return ParseError{{}, 0, 0};
  };

  if (auto prepared = over_pages(prepare_page); !prepared) {
    return std::unexpected(prepared.error());
  }

  // Each related entry carries the summary and title of another post, which
  // makes it a dependency of every page sharing a term with that post. A site
  // whose layouts never ask for related posts pays neither the scoring nor the
  // invalidation.
  if (store->mentions("related")) {
    assign_related(pages, config);
  }

  // A post's related block is no more a function of its own file than its
  // neighbours are: adding, removing, retitling, or retagging one post changes
  // the related block of every post sharing a term with it. Each page records
  // the block it rendered, and one that no longer matches is read and rendered
  // again even though its own file did not change.
  for (std::size_t index = 0; index < pages.size(); ++index) {
    Page& page = pages[index];

    if (page.parsed) {
      continue;
    }

    const std::filesystem::path& file = sources[source_of_page[index]];
    const std::string key = std::filesystem::relative(file, options.content).generic_string();
    const auto known = previous_state.sources.find(key);

    const bool settled = known != previous_state.sources.end() &&
                         known->second.metadata["related-key"].as_string() == related_key(page.related);

    if (settled && writer.keep(page.output)) {
      continue;
    }

    auto post = Post::load(file);

    if (!post) {
      return std::unexpected(post.error());
    }

    page.post = std::move(*post);
    page.tags = Value::array();
    page.parsed = true;

    if (ParseError error = prepare_page(index); error.line != 0) {
      return std::unexpected(error);
    }
  }

  const auto render_page = [&](std::size_t index) {
    Page& page = pages[index];

    if (!page.parsed) {
      return ParseError{{}, 0, 0};
    }

    Arena arena;

    const RenderResult body = render_document(parse_markdown(arena, page.post.body), render_options);

    PostView view_page;
    view_page.chrome.site = Value::object();
    view_page.chrome.site.set("title", Value(config.title));
    view_page.chrome.site.set("base-url", Value(config.base_url));
    view_page.chrome.site.set("author", Value(config.author));
    view_page.chrome.framework = framework;
    view_page.chrome.debug = config.debug;
    view_page.chrome.section = page.section;
    view_page.chrome.url = page.url;
    view_page.chrome.nav = nav;
    view_page.chrome.languages = language_switcher(options, page.translation_key, clean_urls);
    view_page.chrome.has_header = store->has_partial("header", page.section);
    view_page.chrome.has_sidebar = store->has_partial("sidebar", page.section);
    view_page.chrome.has_footer = store->has_partial("footer", page.section);

    auto resolved = data::resolve(*global_data, options.content, page.section);

    view_page.chrome.data = resolved ? *resolved : Value::object();

    view_page.post = &page.post;
    view_page.body_html = body.html;
    view_page.summary = page.summary;
    view_page.headings = body.headings;
    view_page.word_count = page.word_count;
    view_page.reading_time = page.reading_time;
    view_page.tags = page.tags;
    view_page.related = page.related;
    view_page.previous_url = page.previous_url;
    view_page.previous_title = page.previous_title;
    view_page.next_url = page.next_url;
    view_page.next_title = page.next_title;

    if (const SectionConfig* section = config.section(page.section); section != nullptr) {
      if (section->show_dates.has_value()) {
        view_page.show_dates = *section->show_dates;
      }

      // The related list on a post page renders through the same entry partial
      // the section's listing uses, so it follows the section's listing setting.
      if (section->index_dates.has_value()) {
        view_page.index_dates = *section->index_dates;
      }
    }

    ViewContext context = view::build(view_page);

    haml::RenderOptions haml_options;
    haml_options.fragments = &fragments;
    haml_options.partial = [&store, &page](std::string_view name) {
      return store->find_partial(name, page.section);
    };

    // A post picks its own layout, then its section's, then the default. The
    // section's is what resolving from the section's directory upward gives.
    const haml::Template* show = nullptr;

    for (const std::string& candidate : {page.post.layout, std::string("show")}) {
      if (!candidate.empty() && store->has(candidate, page.section)) {
        show = store->find(candidate, page.section);
        break;
      }
    }

    if (show == nullptr) {
      return ParseError{"no 'show' layout found", 1, 1};
    }

    auto inner = haml::render(*show, context, haml_options);

    if (!inner) {
      return inner.error();
    }

    const haml::Template* base = store->find("base", page.section);

    std::string html = *inner;

    if (base != nullptr) {
      haml_options.body = *inner;

      auto wrapped = haml::render(*base, context, haml_options);

      if (!wrapped) {
        return wrapped.error();
      }

      html = *wrapped;
    }

    writer.write(page.output, html);

    // The body and its arena go here. Metadata stays, because listings and the
    // search index read it afterward.
    page.post.body.clear();
    page.post.body.shrink_to_fit();

    count(Counter::pages_rendered);

    return ParseError{{}, 0, 0};
  };

  if (auto rendered = over_pages(render_page); !rendered) {
    return std::unexpected(rendered.error());
  }

  for (std::size_t index = 0; index < pages.size(); ++index) {
    const Page& page = pages[index];

    if (!page.parsed) {
      continue;
    }

    const std::size_t source_index = source_of_page[index];
    const std::filesystem::path& source = sources[source_index];
    const std::string key = std::filesystem::relative(source, options.content).generic_string();
    const auto [size, modified] = stamps[source_index];

    SourceState entry;
    entry.size = size;
    entry.modified = modified;

    // Only a file written during this build needs its bytes remembered. Every
    // other one is settled by the time the next build asks.
    if (modified >= state.started) {
      entry.hash = source_hash(source);
    }

    entry.output = page.output.generic_string();
    entry.metadata = metadata_of(page);

    state.sources.insert_or_assign(key, std::move(entry));
  }

  BuildReport report;
  report.pages = pages.size();

  for (const Page& page : pages) {
    if (page.parsed) {
      ++report.parsed;
      ++report.rendered;
    }
  }

  // Every url the site publishes, gathered as it is produced. Deriving this
  // from what the writer changed would empty the sitemap on a rebuild that
  // changed nothing.
  std::vector<std::string> locations;

  locations.reserve(pages.size());
  for (const Page& page : pages) {
    locations.push_back(config.base_url + page.url);
  }

  // Nothing was added, removed, or reparsed, so every listing, feed, and index
  // would be rendered to the bytes already on disk. What the last build
  // produced is carried forward instead.
  const bool derived_reusable = reusable && report.parsed == 0 &&
                                state.sources.size() == previous_state.sources.size();

  const std::size_t derived_mark = writer.mark();

  if (derived_reusable) {
    for (const std::string& produced : previous_state.derived) {
      if (!writer.keep(options.output / produced)) {
        return std::unexpected(
          ParseError{std::format("cannot reuse '{}'. Build with --force", produced), 1, 1});
      }
    }

    state.derived = previous_state.derived;
    report.listings = static_cast<std::size_t>(previous_state.listings);
  } else {
    // Listings, taxonomies, feeds, and the index all read the metadata the render
    // loop left behind.
    std::map<std::string, std::vector<const Page*>> by_section;

    for (const Page& page : pages) {
      if (!page.section.empty()) {
        by_section[page.section].push_back(&page);
      }
    }

    const auto chrome_for = [&](std::string_view section) {
      Chrome chrome;
      chrome.site = Value::object();
      chrome.site.set("title", Value(config.title));
      chrome.site.set("base-url", Value(config.base_url));
      chrome.site.set("author", Value(config.author));
      chrome.framework = framework;
      chrome.debug = config.debug;
      chrome.section = std::string(section);
      chrome.nav = nav;
      chrome.languages = section_switcher(options, section, clean_urls);
      chrome.has_header = store->has_partial("header", section);
      chrome.has_sidebar = store->has_partial("sidebar", section);
      chrome.has_footer = store->has_partial("footer", section);

      if (auto resolved = data::resolve(*global_data, options.content, section)) {
        chrome.data = *resolved;
      }

      return chrome;
    };

    // A partial is looked up from the section being rendered, so two sections
    // can each have one of their own under the same name.
    const auto options_for_section = [&](std::string_view section) {
      haml::RenderOptions haml_options;
      haml_options.fragments = &fragments;
      haml_options.partial = [&store, section = std::string(section)](std::string_view name) {
        return store->find_partial(name, section);
      };

      return haml_options;
    };

    const auto write_listing = [&](std::string_view section, std::vector<const Page*> entries, bool at_root,
                                   std::string_view heading,
                                   const std::vector<std::string>& template_names) -> std::expected<void, ParseError> {
      // NOLINTNEXTLINE(bugprone-nondeterministic-pointer-iteration-order) ordered by page, not address
      std::stable_sort(entries.begin(), entries.end(),
                       [](const Page* left, const Page* right) { return newer_first(*left, *right); });

      int page_size = config.page_size;

      if (const SectionConfig* configured = config.section(section);
          configured != nullptr && configured->page_size.has_value()) {
        page_size = *configured->page_size;
      }

      page_size = std::max(1, page_size);

      const haml::RenderOptions haml_options = options_for_section(section);

      const auto total = std::max<std::size_t>(
        1, (entries.size() + static_cast<std::size_t>(page_size) - 1) / static_cast<std::size_t>(page_size));

      std::vector<std::string> page_urls;

      for (std::size_t number = 1; number <= total; ++number) {
        page_urls.push_back(
          listing_url(section, static_cast<int>(number), at_root, clean_urls, options.url_prefix));
      }

      for (std::size_t number = 1; number <= total; ++number) {
        ListingView listing;
        listing.chrome = chrome_for(section);
        listing.chrome.url = page_urls[number - 1];
        listing.page_number = static_cast<int>(number);
        listing.total_pages = static_cast<int>(total);
        listing.page_urls = page_urls;
        listing.at_root = at_root;
        listing.heading = std::string(heading);

        if (const SectionConfig* configured = config.section(section);
            configured != nullptr && configured->index_dates.has_value()) {
          listing.index_dates = *configured->index_dates;
        }

        if (number > 1) {
          listing.previous_url = page_urls[number - 2];
        }

        if (number < total) {
          listing.next_url = page_urls[number];
        }

        const std::size_t first = (number - 1) * static_cast<std::size_t>(page_size);
        const std::size_t last = std::min(entries.size(), first + static_cast<std::size_t>(page_size));

        for (std::size_t index = first; index < last; ++index) {
          listing.entries.push(entry_of(*entries[index]));
        }

        const haml::Template* layout = nullptr;

        for (const std::string& name : template_names) {
          if (store->has(name, section)) {
            layout = store->find(name, section);
            break;
          }
        }

        if (layout == nullptr) {
          return std::unexpected(ParseError{"no listing layout found", 1, 1});
        }

        ViewContext context = view::build(listing);

        auto inner = haml::render(*layout, context, haml_options);

        if (!inner) {
          return std::unexpected(inner.error());
        }

        std::string html = *inner;

        if (const haml::Template* base = store->find("base", section)) {
          haml::RenderOptions wrapped_options = haml_options;
          wrapped_options.body = *inner;

          auto wrapped = haml::render(*base, context, wrapped_options);

          if (!wrapped) {
            return std::unexpected(wrapped.error());
          }

          html = *wrapped;
        }

        writer.write(listing_file(options.output, section, static_cast<int>(number), at_root, clean_urls),
                     html);

        locations.push_back(config.base_url + page_urls[number - 1]);
        ++report.listings;
      }

      return {};
    };

    for (const auto& entry : by_section) {
      if (auto result = write_listing(entry.first, entry.second, false, {}, {"index"}); !result) {
        return std::unexpected(result.error());
      }
    }

    if (!config.home_section.empty() && by_section.contains(config.home_section)) {
      if (auto result = write_listing(config.home_section, by_section.at(config.home_section), true, {},
                                      {"home", "index"});
          !result) {
        return std::unexpected(result.error());
      }
    }

    // Taxonomy term pages and their index.
    for (const std::string& taxonomy : config.taxonomies) {
      std::map<std::string, std::vector<const Page*>> by_term;

      for (const Page& page : pages) {
        for (const std::string& term : page.post.terms(taxonomy)) {
          by_term[term].push_back(&page);
        }
      }

      if (by_term.empty()) {
        continue;
      }

      for (const auto& entry : by_term) {
        const std::string path = taxonomy + "/" + slug::slugify(entry.first);

        if (auto result = write_listing(path, entry.second, false, entry.first,
                                        {taxonomy.substr(0, taxonomy.size() - (taxonomy.ends_with('s') ? 1 : 0)),
                                         "term", taxonomy, "index"});
            !result) {
          return std::unexpected(result.error());
        }
      }

      std::vector<const Page*> none;

      ListingView index;
      index.chrome = chrome_for(taxonomy);
      index.chrome.url = join_url({}, taxonomy, clean_urls);
      index.heading = slug::humanize(taxonomy);
      index.page_urls = {index.chrome.url};

      for (const auto& entry : by_term) {
        Value term = Value::object();
        term.set("title", Value(std::format("{} ({})", entry.first, entry.second.size())));
        term.set("url", Value(join_url({}, taxonomy + "/" + slug::slugify(entry.first), clean_urls)));
        term.set("date", Value(std::string{}));

        index.entries.push(std::move(term));
      }

      const haml::Template* layout = store->has(taxonomy) ? store->find(taxonomy) : store->find("index");

      if (layout != nullptr) {
        const haml::RenderOptions haml_options = options_for_section({});

        ViewContext context = view::build(index);

        auto inner = haml::render(*layout, context, haml_options);

        if (!inner) {
          return std::unexpected(inner.error());
        }

        std::string html = *inner;

        if (const haml::Template* base = store->find("base")) {
          haml::RenderOptions wrapped_options = haml_options;
          wrapped_options.body = *inner;

          auto wrapped = haml::render(*base, context, wrapped_options);

          if (!wrapped) {
            return std::unexpected(wrapped.error());
          }

          html = *wrapped;
        }

        writer.write(clean_urls ? options.output / (taxonomy + ".html")
                                : options.output / taxonomy / "index.html",
                     html);

        locations.push_back(config.base_url + index.chrome.url);
        ++report.listings;
      }
    }

    if (!pages.empty()) {
      std::vector<const Page*> newest;

      newest.reserve(pages.size());
      for (const Page& page : pages) {
        newest.push_back(&page);
      }

      // NOLINTNEXTLINE(bugprone-nondeterministic-pointer-iteration-order) ordered by page, not address
      std::stable_sort(newest.begin(), newest.end(),
                       [](const Page* left, const Page* right) { return newer_first(*left, *right); });

      for (const std::string& format : config.feed_formats) {
        FeedInfo info;
        info.title = config.title;
        info.site_url = config.base_url + options.url_prefix + "/";
        info.feed_url =
          config.base_url + options.url_prefix + "/" + std::string(feed::filename_for(format));
        info.updated = newest.empty() ? std::string{} : newest.front()->post.date_string();

        for (const Page* page : newest) {
          info.entries.push_back(
            FeedEntry{page->post.title, config.base_url + page->url, page->post.date_string(), page->summary});
        }

        const std::string body = format == "rss"    ? feed::rss(info)
                                 : format == "json" ? feed::json_feed(info)
                                                    : feed::atom(info);

        if (!shipped.contains(std::string(feed::filename_for(format)))) {
          writer.write(options.output / feed::filename_for(format), body);
        }

        // A feed per section as well, so a reader can follow one part of a site
        // without taking all of it.
        for (const auto& entry : by_section) {
          std::vector<const Page*> members = entry.second;

          // NOLINTNEXTLINE(bugprone-nondeterministic-pointer-iteration-order) ordered by page, not address
          std::stable_sort(members.begin(), members.end(), [](const Page* left, const Page* right) {
            return newer_first(*left, *right);
          });

          const std::string prefix = options.url_prefix + "/" + entry.first;

          FeedInfo section_feed;
          section_feed.title = config.title + ": " + entry.first;
          section_feed.site_url = config.base_url + prefix;
          section_feed.feed_url =
            config.base_url + prefix + "/" + std::string(feed::filename_for(format));
          section_feed.updated = members.front()->post.date_string();

          for (const Page* page : members) {
            section_feed.entries.push_back(FeedEntry{page->post.title, config.base_url + page->url,
                                                     page->post.date_string(), page->summary});
          }

          const std::string section_body = format == "rss" ? feed::rss(section_feed)
                                           : format == "json"
                                             ? feed::json_feed(section_feed)
                                             : feed::atom(section_feed);

          const std::filesystem::path relative =
            std::filesystem::path(entry.first) / feed::filename_for(format);

          if (shipped.contains(relative.generic_string())) {
            continue;
          }

          writer.write(options.output / relative, section_body);
        }
      }

      std::sort(locations.begin(), locations.end());
      locations.erase(std::unique(locations.begin(), locations.end()), locations.end());

      if (!shipped.contains("sitemap.xml")) {
        writer.write(options.output / "sitemap.xml", feed::sitemap(locations));
      }

      if (config.robots && !shipped.contains("robots.txt")) {
        writer.write(options.output / "robots.txt", feed::robots_txt(config.base_url));
      }

      if (config.search) {
        std::vector<SearchRecord> records;

        records.reserve(pages.size());
        for (const Page& page : pages) {
          records.push_back(SearchRecord{page.post.title, page.url, page.post.date_string(),
                                         page.post.description, page.summary, page.post.tags});
        }

        // The stylesheet and the script are planned with the site's own assets,
        // since a page has to know their names before it is written.
        writer.write(options.output / "search-index.json",
                     search::index_json(records, static_cast<std::size_t>(config.search_text_length)));
      }
    }

    // The page a static host serves for a url it does not have. It is a listing
    // with nothing in it, so a site with a 404 layout gets its own chrome around
    // the message and one without gets a plain page.
    if (!shipped.contains("404.html")) {
      std::string html = not_found_html();

      if (store->has("404")) {
        const haml::RenderOptions haml_options = options_for_section({});

        ListingView listing;
        listing.chrome = chrome_for("");
        listing.chrome.url = options.url_prefix + "/404";

        ViewContext context = view::build(listing);

        auto inner = haml::render(*store->find("404"), context, haml_options);

        if (!inner) {
          return std::unexpected(inner.error());
        }

        html = *inner;

        if (const haml::Template* base = store->find("base")) {
          haml::RenderOptions wrapped_options = haml_options;
          wrapped_options.body = *inner;

          auto wrapped = haml::render(*base, context, wrapped_options);

          if (!wrapped) {
            return std::unexpected(wrapped.error());
          }

          html = *wrapped;
        }
      }

      writer.write(options.output / "404.html", html);
    }

    // A url a post used to live at sends a browser to where it lives now. A
    // page of the site's own always wins the path, so an alias can never
    // replace something the content tree publishes.
    std::map<std::string, std::string> aliased;

    for (const Page& page : pages) {
      for (const std::string& alias : page.post.aliases) {
        const std::filesystem::path file = url_to_file(options.output, alias, clean_urls);
        const std::string key = file.generic_string();

        if (seen.contains(key)) {
          continue;
        }

        // Two posts claiming one alias would each redirect it somewhere else.
        // The first keeps it, and the second is said out loud, since a silently
        // dropped redirect looks exactly like one that works.
        if (const auto claimed = aliased.find(key); claimed != aliased.end()) {
          warnings.push_back(std::format("alias '{}' is claimed by both '{}' and '{}', so it points at the first",
                                         alias, claimed->second, page.url));
          continue;
        }

        aliased.emplace(key, page.url);

        writer.write(file, redirect_html(page.url));
      }
    }

    state.derived = writer.since(derived_mark);
  }

  state.listings = static_cast<std::int64_t>(report.listings);

  // Static files land at the site root, and assets land under assets/.
  // Copies each chosen file to the path it was chosen for. One entry per
  // published path, so nothing is ever written twice.
  const auto copy_chosen = [&](const std::map<std::filesystem::path, std::filesystem::path>& chosen,
                               const std::filesystem::path& to) {
    for (const auto& source : chosen) {
      const std::filesystem::path& file = source.second;
      const std::filesystem::path destination = to / source.first;
      const std::string key = std::filesystem::relative(destination, options.output).generic_string();
      const auto [size, modified] = file_stamp(file);

      const auto known = previous_state.copies.find(key);

      // An image that has not changed should not be read, and hashing one to
      // find that out costs about what copying it costs.
      const bool stamped = !options.force && known != previous_state.copies.end() &&
                           known->second.size == size && known->second.modified == modified;

      if (stamped && (previous_state.settled(known->second) || source_hash(file) == known->second.hash) &&
          writer.keep(destination)) {
        state.copies.emplace(key, known->second);

        continue;
      }

      SourceState entry;
      entry.size = size;
      entry.modified = modified;

      if (modified >= state.started) {
        entry.hash = source_hash(file);
      }

      writer.copy(file, destination);
      state.copies.insert_or_assign(key, std::move(entry));
    }
  };

  // One file per published path, chosen before anything is written. Copying the
  // theme's tree and then the site's would write two different things to one
  // path and leave the order to decide which survived.
  copy_chosen(static_sources, options.output);

  for (const PlannedAsset& asset : plan.assets) {
    const std::filesystem::path destination = options.output / asset.destination;

    // A reused asset was never read, so there is nothing to write. If it turns
    // out not to be on disk after all, it is read and written like any other.
    if (asset.reused && writer.keep(destination)) {
      ++report.assets_reused;

      continue;
    }

    if (asset.reused) {
      writer.copy(asset.source, destination);

      continue;
    }

    writer.write(destination, asset.content);
  }

  if (!writer.collisions().empty()) {
    const std::vector<std::string>& clashing = writer.collisions();

    return std::unexpected(ParseError{
      std::format("two different results were written to '{}'{}", clashing.front(),
                  clashing.size() > 1 ? std::format(" (and {} more)", clashing.size() - 1) : ""),
      1, 1});
  }

  writer.prune();
  writer.save_manifest();
  state.save(state_path);

  report.written = writer.written();
  report.skipped = writer.skipped();
  report.fragments_reused = fragments.hits();
  report.fragments_rendered = fragments.renders();
  report.changed = writer.changed();
  report.warnings = plan.warnings;
  report.warnings.insert(report.warnings.end(), warnings.begin(), warnings.end());

  return report;
}

}  // namespace blogin
