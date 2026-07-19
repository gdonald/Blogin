#include <string>
#include <vector>

#include "search.h"
#include "support/spec.h"

using blogin::SearchRecord;
using spec::expect;

namespace {

std::vector<SearchRecord> corpus() {
  return {
    SearchRecord{"Raku grammars", "/raku", "2024-01-01", "", "Parsing text with grammars.", {"raku"}},
    SearchRecord{"Testing in C++", "/cpp", "2024-02-01", "", "Grammars barely appear here.", {"cpp", "testing"}},
    SearchRecord{"Unrelated", "/other", "2024-03-01", "", "Nothing to see.", {}},
  };
}

}  // namespace

SPEC {
  spec::describe("search", [] {
    spec::context("tokenizing", [] {
      spec::it("splits on punctuation", [] {
        expect(blogin::search::tokenize("one, two.").size()).to_eq(std::size_t{2});
      });

      spec::it("lowercases", [] { expect(blogin::search::tokenize("Raku")[0]).to_eq("raku"); });

      spec::it("keeps digits", [] { expect(blogin::search::tokenize("top 10")[1]).to_eq("10"); });

      spec::it("yields nothing for punctuation alone", [] {
        expect(blogin::search::tokenize("!!!").size()).to_eq(std::size_t{0});
      });
    });

    spec::context("ranking", [] {
      // A title match counting for ten and a body match for one is what makes a
      // small index feel accurate rather than arbitrary.
      spec::it("puts a title match above a body match", [] {
        const auto results = blogin::search::rank(corpus(), "grammars");

        expect(results[0].url).to_eq("/raku");
      });

      spec::it("finds a match anywhere", [] {
        expect(blogin::search::rank(corpus(), "grammars").size()).to_eq(std::size_t{2});
      });

      spec::it("matches a prefix", [] {
        expect(blogin::search::rank(corpus(), "gram").size()).to_eq(std::size_t{2});
      });

      spec::it("matches a tag", [] {
        expect(blogin::search::rank(corpus(), "testing")[0].url).to_eq("/cpp");
      });

      spec::it("leaves out what does not match", [] {
        expect(blogin::search::rank(corpus(), "grammars")[0].url).not_to_eq("/other");
      });

      spec::it("finds nothing for an empty query", [] {
        expect(blogin::search::rank(corpus(), "").size()).to_eq(std::size_t{0});
      });

      spec::it("respects the cap", [] {
        expect(blogin::search::rank(corpus(), "grammars", 1).size()).to_eq(std::size_t{1});
      });
    });

    spec::context("the index", [] {
      auto json = spec::let([] { return blogin::search::index_json(corpus()); });

      spec::it("carries each record", [=] { expect(json()).to_contain("Raku grammars"); });

      spec::it("carries the tags", [=] { expect(json()).to_contain("\"raku\""); });

      // Sorted keys keep the file stable whatever order posts were found in.
      spec::it("sorts the keys of a record", [=] {
        expect(json()).to_contain("{\"date\":");
      });

      spec::it("caps the text it stores", [] {
        const std::vector<SearchRecord> long_post{
          SearchRecord{"T", "/t", "", "", std::string(5000, 'x'), {}},
        };

        expect(blogin::search::index_json(long_post, 100).size()).to_be_less_than(std::size_t{400});
      });
    });

    spec::context("the assets it emits", [] {
      spec::it("writes the cap into the script", [] {
        expect(blogin::search::script(25)).to_contain("BLOGIN_SEARCH_CAP = 25");
      });

      spec::it("emits the ranking the index was built for", [] {
        expect(blogin::search::script()).to_contain("WEIGHT");
      });

      spec::it("emits a stylesheet for the results", [] {
        expect(std::string(blogin::search::stylesheet())).to_contain("data-blogin-results");
      });
    });
  });
}
