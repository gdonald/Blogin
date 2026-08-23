#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <utility>
#include <vector>

#include "config.h"
#include "json.h"
#include "support/spec.h"

using blogin::Config;
using spec::expect;

namespace {

Config from(std::string_view json) {
  return Config::from_value(blogin::parse_json(json).value()).value_or(Config{});
}

std::string error_from(std::string_view json) {
  const auto config = Config::from_value(blogin::parse_json(json).value());

  return config ? std::string("no error") : config.error().message;
}

bool rejects(std::string_view json) {
  return !Config::from_value(blogin::parse_json(json).value()).has_value();
}

}  // namespace

SPEC {
  spec::describe("Config", [] {
    spec::context("defaults", [] {
      auto config = spec::let([] { return Config{}; });

      spec::it("writes to public", [=] { expect(config().output_dir).to_eq("public"); });

      spec::it("pages at ten", [=] { expect(config().page_size).to_eq(10); });

      spec::it("uses no css framework", [=] { expect(config().css_framework).to_eq("none"); });

      spec::it("enables search", [=] { expect(config().search).to_be_true(); });

      spec::it("enables robots", [=] { expect(config().robots).to_be_true(); });

      spec::it("leaves clean urls off", [=] { expect(config().clean_urls).to_be_false(); });

      spec::it("taxonomises by tags", [=] { expect(config().taxonomies.size()).to_eq(std::size_t{1}); });

      spec::it("publishes an atom feed", [=] { expect(config().feed_formats[0]).to_eq("atom"); });
    });

    spec::context("reading values", [] {
      spec::it("reads a string", [] {
        expect(from(R"({"title":"Blogin"})").title).to_eq("Blogin");
      });

      spec::it("reads an integer", [] { expect(from(R"({"page-size":25})").page_size).to_eq(25); });

      spec::it("reads a boolean", [] { expect(from(R"({"clean-urls":true})").clean_urls).to_be_true(); });

      spec::it("reads a list of strings", [] {
        expect(from(R"({"taxonomies":["tags","topics"]})").taxonomies.size()).to_eq(std::size_t{2});
      });

      spec::it("reads a list of integers", [] {
        expect(from(R"({"image-widths":[400,800]})").image_widths.size()).to_eq(std::size_t{2});
      });

      spec::it("reads every feed format it knows", [] {
        expect(from(R"({"feed-formats":["atom","rss","json"]})").feed_formats.size()).to_eq(std::size_t{3});
      });
    });

    // A setting that silently does nothing is worse than one that complains.
  });
}

SPEC {
  spec::describe("Config", [] {
    spec::context("rejecting the wrong type", [] {
      spec::it("rejects a number where a string belongs", [] {
        expect(rejects(R"({"title":1})")).to_be_true();
      });

      spec::it("rejects a string where an integer belongs", [] {
        expect(rejects(R"({"page-size":"ten"})")).to_be_true();
      });

      spec::it("rejects a string where a boolean belongs", [] {
        expect(rejects(R"({"debug":"yes"})")).to_be_true();
      });

      spec::it("rejects a scalar where a list belongs", [] {
        expect(rejects(R"({"taxonomies":"tags"})")).to_be_true();
      });

      spec::it("rejects a list holding the wrong type", [] {
        expect(rejects(R"({"taxonomies":["tags",1]})")).to_be_true();
      });

      spec::it("rejects an unknown feed format", [] {
        expect(rejects(R"({"feed-formats":["carrier-pigeon"]})")).to_be_true();
      });

      spec::it("names the key it rejected", [] {
        expect(error_from(R"({"page-size":"ten"})")).to_contain("page-size");
      });

      spec::it("says what the type should have been", [] {
        expect(error_from(R"({"page-size":"ten"})")).to_contain("must be an integer");
      });

      spec::it("says what the type actually was", [] {
        expect(error_from(R"({"page-size":"ten"})")).to_contain("not string");
      });

      spec::it("rejects a scalar where a list of integers belongs", [] {
        expect(rejects(R"({"image-widths":480})")).to_be_true();
      });

      spec::it("rejects a list of integers holding something else", [] {
        expect(rejects(R"({"image-widths":[480,"wide"]})")).to_be_true();
      });

      spec::it("rejects a scalar where a list of formats belongs", [] {
        expect(rejects(R"({"feed-formats":"atom"})")).to_be_true();
      });

      spec::it("rejects language config that is not a map", [] {
        expect(rejects(R"({"language-config":5})")).to_be_true();
      });

      spec::it("rejects a language title that is not a string", [] {
        expect(rejects(R"({"language-config":{"fr":{"title":5}}})")).to_be_true();
      });

      spec::it("rejects sections that are not a map", [] {
        expect(rejects(R"({"sections":5})")).to_be_true();
      });

      spec::it("lists the formats it knows", [] {
        expect(error_from(R"({"feed-formats":["carrier-pigeon"]})")).to_contain("atom, rss, or json");
      });
    });

    // Silently ignoring a key it does not recognise would make a typo read as
    // "my setting does nothing" and cost an afternoon to find.
  });
}

SPEC {
  spec::describe("Config", [] {
    spec::context("unknown keys", [] {
      spec::it("collects them rather than failing", [] {
        expect(from(R"({"pagesize":10})").unknown_keys.size()).to_eq(std::size_t{1});
      });

      spec::it("names the one it did not know", [] {
        expect(from(R"({"pagesize":10})").unknown_keys[0]).to_eq("pagesize");
      });

      spec::it("still reads the keys it does know", [] {
        expect(from(R"({"pagesize":10,"title":"Blogin"})").title).to_eq("Blogin");
      });

      spec::it("suggests the key a typo was probably meant to be", [] {
        expect(blogin::nearest_key_hint("page-siz")).to_contain("page-size");
      });

      spec::it("suggests nothing for a key close to nothing", [] {
        expect(blogin::nearest_key_hint("completely-unrelated-nonsense")).to_eq("");
      });

      spec::it("carries the plugins key as unknown", [] {
        expect(from(R"({"plugins":["Some::Module"]})").unknown_keys[0]).to_eq("plugins");
      });
    });

  });
}

SPEC {
  spec::describe("Config", [] {
    spec::context("sections", [] {
      auto config = spec::let([] {
        return from(R"({"sections":{"posts":{"page-size":5,"label":"Writing","nav":false}}})");
      });

      spec::it("reads a section", [=] { expect(config().sections.size()).to_eq(std::size_t{1}); });

      spec::it("finds a section by name", [=] {
        expect(config().section("posts") != nullptr).to_be_true();
      });

      spec::it("reads a section page size", [=] {
        expect(config().section("posts")->page_size.value()).to_eq(5);
      });

      spec::it("reads a section label", [=] {
        expect(config().section("posts")->label.value()).to_eq("Writing");
      });

      spec::it("reads a section nav flag", [=] {
        expect(config().section("posts")->nav.value()).to_be_false();
      });

      spec::it("leaves an unset section field empty", [=] {
        expect(config().section("posts")->layout.has_value()).to_be_false();
      });

      spec::it("finds nothing for a section that is not configured", [=] {
        expect(config().section("absent") == nullptr).to_be_true();
      });

      spec::it("rejects a section that is not a map", [] {
        expect(rejects(R"({"sections":{"posts":5}})")).to_be_true();
      });

      spec::it("rejects a section field of the wrong type", [] {
        expect(rejects(R"({"sections":{"posts":{"page-size":"five"}}})")).to_be_true();
      });

      const std::vector<std::pair<std::string, std::string>> section_fields{
        {"label", "5"},
        {"order", R"("first")"},
        {"nav", R"("yes")"},
        {"layout", "5"},
        {"index-dates", R"("yes")"},
        {"show-dates", R"("yes")"},
      };

      for (const auto& example : section_fields) {
        spec::it("rejects a section " + example.first + " of the wrong type", [example] {
          expect(rejects(R"({"sections":{"posts":{")" + example.first + R"(":)" + example.second + "}}}"))
            .to_be_true();
        });
      }

      spec::it("reads every field a section can carry", [] {
        const Config carried = from(
          R"({"sections":{"posts":{"label":"Writing","order":2,"nav":true,"layout":"wide",)"
          R"("index-dates":false,"show-dates":false}}})");

        spec::aggregate_failures([&] {
          expect(carried.section("posts")->order.value()).to_eq(2);
          expect(carried.section("posts")->layout.value()).to_eq("wide");
          expect(carried.section("posts")->index_dates.value()).to_be_false();
          expect(carried.section("posts")->show_dates.value()).to_be_false();
        });
      });

      spec::it("names the section field it rejected", [] {
        expect(error_from(R"({"sections":{"posts":{"page-size":"five"}}})")).to_contain("sections.posts.page-size");
      });
    });

  });
}

SPEC {
  spec::describe("Config", [] {
    spec::context("language config", [] {
      spec::it("reads a language entry", [] {
        expect(from(R"({"language-config":{"fr":{"title":"Blogin FR"}}})").language_config.size())
          .to_eq(std::size_t{1});
      });

      spec::it("reads a language title", [] {
        expect(from(R"({"language-config":{"fr":{"title":"Blogin FR"}}})").language_config[0].title.value())
          .to_eq("Blogin FR");
      });

      spec::it("rejects a language entry that is not a map", [] {
        expect(rejects(R"({"language-config":{"fr":"nope"}})")).to_be_true();
      });
    });

    spec::context("loading a file", [] {
      spec::it("falls back to the defaults when there is none", [] {
        expect(Config::load("/nonexistent/blogin.json").value().output_dir).to_eq("public");
      });

      spec::it("reads the file it was given", [] {
        const std::filesystem::path root = spec::scratch_directory("config");
        std::filesystem::create_directories(root);

        std::ofstream(root / "blogin.json", std::ios::binary) << R"({"output-dir":"site"})";

        expect(Config::load(root / "blogin.json").value().output_dir).to_eq("site");
      });

      spec::it("refuses a file that is not json", [] {
        const std::filesystem::path root = spec::scratch_directory("config");
        std::filesystem::create_directories(root);

        std::ofstream(root / "blogin.json", std::ios::binary) << "{not json";

        expect(Config::load(root / "blogin.json").has_value()).to_be_false();
      });
    });

    spec::it("rejects a document that is not an object", [] {
      expect(rejects("[1,2]")).to_be_true();
    });
  });
}
