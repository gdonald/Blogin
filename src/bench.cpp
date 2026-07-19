#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "arena.h"
#include "build.h"
#include "config.h"
#include "counters.h"
#include "html.h"
#include "markdown.h"
#include "site.h"
#include "template.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Measurement {
  std::string label;
  double milliseconds = 0.0;
  std::size_t items = 0;
  std::size_t bytes = 0;
};

std::vector<std::filesystem::path> files_with_extension(const std::filesystem::path& root,
                                                        std::string_view extension) {
  std::vector<std::filesystem::path> found;

  if (!std::filesystem::exists(root)) {
    return found;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      found.push_back(entry.path());
    }
  }

  std::sort(found.begin(), found.end());

  return found;
}

std::vector<std::string> read_all(const std::vector<std::filesystem::path>& paths) {
  std::vector<std::string> contents;
  contents.reserve(paths.size());

  for (const std::filesystem::path& path : paths) {
    contents.push_back(blogin::read_file(path));
  }

  return contents;
}

template <typename Body>
Measurement measure(std::string label, std::size_t items, std::size_t bytes, Body body) {
  const auto started = Clock::now();

  body();

  const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started);

  return Measurement{std::move(label), elapsed.count(), items, bytes};
}

// Directories under the corpus that are sites in their own right.
std::vector<std::filesystem::path> sites_under(const std::filesystem::path& root) {
  std::vector<std::filesystem::path> sites;

  if (!std::filesystem::exists(root)) {
    return sites;
  }

  for (const auto& entry : std::filesystem::directory_iterator(root)) {
    if (entry.is_directory() && std::filesystem::exists(entry.path() / "blogin.json")) {
      sites.push_back(entry.path());
    }
  }

  std::sort(sites.begin(), sites.end());

  return sites;
}

std::size_t total_bytes(const std::vector<std::string>& contents) {
  std::size_t total = 0;

  for (const std::string& content : contents) {
    total += content.size();
  }

  return total;
}

Measurement parse_markdown_bench(std::string label, const std::vector<std::string>& contents) {
  return measure(std::move(label), contents.size(), total_bytes(contents), [&] {
    for (const std::string& content : contents) {
      const blogin::Source source(content);

      blogin::Arena arena;

      std::string html;
      html.reserve(content.size() * 2);

      blogin::render_html(html, blogin::parse_markdown(arena, source));
    }
  });
}

// The same work with one arena reused across every post, which is what a real
// build does on each worker thread.
Measurement parse_markdown_reused(std::string label, const std::vector<std::string>& contents) {
  return measure(std::move(label), contents.size(), total_bytes(contents), [&] {
    blogin::Arena arena;
    std::string html;

    for (const std::string& content : contents) {
      const blogin::Source source(content);

      html.clear();

      blogin::render_html(html, blogin::parse_markdown(arena, source));

      arena.reset();
    }
  });
}

void report(const std::vector<Measurement>& measurements) {
  std::cout << std::format("{:<34} {:>10} {:>9} {:>12}\n", "what", "ms", "items", "MB/s");
  std::cout << std::string(68, '-') << "\n";

  for (const Measurement& measurement : measurements) {
    const double seconds = measurement.milliseconds / 1000.0;
    const double megabytes = static_cast<double>(measurement.bytes) / (1024.0 * 1024.0);
    const double throughput = seconds > 0.0 ? megabytes / seconds : 0.0;

    std::cout << std::format("{:<34} {:>10.2f} {:>9} {:>12.1f}\n", measurement.label, measurement.milliseconds,
                             measurement.items, throughput);
  }
}

// Wall-clock numbers, recorded so a slowdown is visible over time. Never a
// gate: timing on a shared machine is noise, and the spec suite asserts on work
// counters instead.
int run(const std::vector<std::string>& arguments) {
  // "dump-html <corpus> <out>" writes each post's rendered body, so the same
  // content can be compared against another implementation.
  if (arguments.size() > 3 && arguments[1] == "dump-html") {
    const std::filesystem::path corpus_root = arguments[2];
    const std::filesystem::path target_root = arguments[3];

    for (const std::filesystem::path& path : files_with_extension(corpus_root, ".md")) {
      std::string text = blogin::read_file(path);

      // Front matter is not the parser's business, so it comes off first.
      if (text.starts_with("---\n")) {
        const auto closing = text.find("\n---", 4);

        if (closing != std::string::npos) {
          const auto after = text.find('\n', closing + 1);
          text = after == std::string::npos ? std::string{} : text.substr(after + 1);
        }
      }

      blogin::Arena arena;

      std::string html;
      blogin::render_html(html, blogin::parse_markdown(arena, text));

      const std::filesystem::path target =
        target_root / (std::filesystem::relative(path, corpus_root).string() + ".html");

      blogin::write_file(target, html);
    }

    return 0;
  }

  const std::filesystem::path corpus = arguments.size() > 1 ? arguments[1] : "specs/corpus";
  const std::filesystem::path synthetic = arguments.size() > 2 ? arguments[2] : "build/synth-corpus";

  std::vector<Measurement> measurements;

  const std::vector<std::filesystem::path> corpus_posts = files_with_extension(corpus, ".md");
  const std::vector<std::filesystem::path> corpus_layouts = files_with_extension(corpus, ".haml");

  if (corpus_posts.empty()) {
    std::cerr << "no corpus at " << corpus << "\n";
    return 2;
  }

  const std::vector<std::string> post_sources = read_all(corpus_posts);
  const std::vector<std::string> layout_sources = read_all(corpus_layouts);

  measurements.push_back(parse_markdown_bench("markdown, real corpus", post_sources));
  measurements.push_back(parse_markdown_reused("markdown, real, arena reused", post_sources));

  measurements.push_back(
    measure("template compile, real corpus", layout_sources.size(), total_bytes(layout_sources), [&] {
      for (const std::string& source : layout_sources) {
        const blogin::CompiledTemplate compiled = blogin::CompiledTemplate::compile(source);

        (void)compiled.root();
      }
    }));

  if (!layout_sources.empty()) {
    const blogin::CompiledTemplate compiled = blogin::CompiledTemplate::compile(layout_sources.front());

    blogin::Context context;
    context.set("title", "Benchmark");
    context.set_body("<p>body</p>\n");

    constexpr std::size_t render_count = 10000;

    measurements.push_back(measure("template render, one page x10k", render_count, 0, [&] {
      std::string out;

      for (std::size_t index = 0; index < render_count; ++index) {
        out.clear();
        render_template(out, compiled, context);
      }
    }));
  }

  const std::vector<std::filesystem::path> synthetic_posts = files_with_extension(synthetic, ".md");

  if (!synthetic_posts.empty()) {
    const std::vector<std::string> synthetic_sources = read_all(synthetic_posts);

    measurements.push_back(parse_markdown_bench("markdown, synthetic corpus", synthetic_sources));
    measurements.push_back(parse_markdown_reused("markdown, synthetic, arena reused", synthetic_sources));
  } else {
    std::cerr << "no synthetic corpus at " << synthetic << ". Run scripts/synth-corpus.sh for scaling numbers\n";
  }

  // Whole-site builds, which is the number a person waits on. Each
  // site is copied first, so a run does not depend on what a previous one left
  // behind and the two builds measure a cold start and a rebuild.
  const auto build_bench = [&](const std::filesystem::path& root) {
    const auto config = blogin::Config::load(root / "blogin.json");

    if (!config) {
      return;
    }

    const std::filesystem::path working =
      std::filesystem::temp_directory_path() / "blogin-bench" / root.filename();

    std::filesystem::remove_all(working);
    std::filesystem::create_directories(working.parent_path());
    std::filesystem::copy(root, working, std::filesystem::copy_options::recursive);

    const blogin::BuildOptions options = blogin::BuildOptions::around(working / "content", *config);

    std::size_t pages = 0;

    measurements.push_back(
      measure("build " + root.filename().string(), 0, 0, [&] {
        if (const auto report = blogin::build(options)) {
          pages = report->pages + report->listings;
        }
      }));

    measurements.back().items = pages;

    measurements.push_back(measure("rebuild " + root.filename().string(), pages, 0, [&] {
      if (!blogin::build(options)) {
        std::cerr << "bench: rebuilding " << root.string() << " failed\n";
      }
    }));

    std::filesystem::remove_all(working);
  };

  for (const std::filesystem::path& site : sites_under(corpus)) {
    build_bench(site);
  }

  if (!synthetic_posts.empty()) {
    build_bench(synthetic);
  }

  report(measurements);

  std::cout << "\n" << blogin::counters_report();

  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(std::vector<std::string>(argv, argv + argc));
  } catch (const std::exception& error) {
    std::cerr << "bench: " << error.what() << "\n";

    return 1;
  }
}
