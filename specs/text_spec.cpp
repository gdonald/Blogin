#include <string>

#include "support/spec.h"
#include "text.h"

using spec::expect;

namespace {

// One byte, two bytes, three bytes, four bytes.
constexpr std::string_view ascii = "abc";
constexpr std::string_view accented = "café";
constexpr std::string_view japanese = "こんにちは";
constexpr std::string_view emoji = "a\U0001F600b";

}  // namespace

SPEC {
  spec::describe("text", [] {
    spec::context("counting characters", [] {
      spec::it("counts ascii", [] { expect(blogin::text::char_length(ascii)).to_eq(std::size_t{3}); });

      spec::it("counts a two-byte sequence as one character", [] {
        expect(blogin::text::char_length(accented)).to_eq(std::size_t{4});
      });

      spec::it("counts three-byte sequences as one character each", [] {
        expect(blogin::text::char_length(japanese)).to_eq(std::size_t{5});
      });

      spec::it("counts a four-byte sequence as one character", [] {
        expect(blogin::text::char_length(emoji)).to_eq(std::size_t{3});
      });

      spec::it("counts an empty string as nothing", [] {
        expect(blogin::text::char_length("")).to_eq(std::size_t{0});
      });

      // A post with one bad byte should still build, so malformed input counts
      // the byte and moves on, never rejecting the document.
      spec::it("counts a stray continuation byte as one character", [] {
        expect(blogin::text::char_length("\x80\x80")).to_eq(std::size_t{2});
      });
    });

    spec::context("substring by character", [] {
      spec::it("takes a prefix of ascii", [] {
        expect(std::string(blogin::text::substr(ascii, 0, 2))).to_eq("ab");
      });

      // This is the case that matters: a byte-based substr would cut the
      // accented character in half and emit invalid UTF-8.
      spec::it("does not split a multi-byte character", [] {
        expect(std::string(blogin::text::substr(accented, 0, 4))).to_eq("café");
      });

      spec::it("takes a prefix that stops before a multi-byte character", [] {
        expect(std::string(blogin::text::substr(accented, 0, 3))).to_eq("caf");
      });

      spec::it("takes from the middle", [] {
        expect(std::string(blogin::text::substr(japanese, 1, 2))).to_eq("んに");
      });

      spec::it("takes to the end when no count is given", [] {
        expect(std::string(blogin::text::substr(accented, 3))).to_eq("é");
      });

      spec::it("clamps a count past the end", [] {
        expect(std::string(blogin::text::substr(ascii, 1, 99))).to_eq("bc");
      });

      spec::it("yields nothing when the start is past the end", [] {
        expect(std::string(blogin::text::substr(ascii, 99, 1))).to_eq("");
      });
    });

    spec::context("trimming", [] {
      spec::it("removes leading and trailing whitespace", [] {
        expect(std::string(blogin::text::trim("  hello  "))).to_eq("hello");
      });

      spec::it("removes tabs and newlines", [] {
        expect(std::string(blogin::text::trim("\t\nhello\n\t"))).to_eq("hello");
      });

      spec::it("leaves inner whitespace alone", [] {
        expect(std::string(blogin::text::trim("  a b  "))).to_eq("a b");
      });

      spec::it("yields nothing for whitespace alone", [] {
        expect(std::string(blogin::text::trim("   "))).to_eq("");
      });

      spec::it("trims only the start when asked", [] {
        expect(std::string(blogin::text::trim_start("  a  "))).to_eq("a  ");
      });

      spec::it("trims only the end when asked", [] {
        expect(std::string(blogin::text::trim_end("  a  "))).to_eq("  a");
      });
    });

    spec::context("splitting", [] {
      spec::it("splits on a separator", [] {
        expect(blogin::text::split("a,b,c", ',').size()).to_eq(std::size_t{3});
      });

      spec::it("keeps empty pieces", [] {
        expect(blogin::text::split("a,,c", ',').size()).to_eq(std::size_t{3});
      });

      spec::it("yields the whole string when the separator is absent", [] {
        expect(std::string(blogin::text::split("abc", ',')[0])).to_eq("abc");
      });

      spec::it("splits lines", [] {
        expect(blogin::text::split_lines("one\ntwo\nthree").size()).to_eq(std::size_t{3});
      });

      spec::it("drops a carriage return before a newline", [] {
        expect(std::string(blogin::text::split_lines("one\r\ntwo")[0])).to_eq("one");
      });

      spec::it("ignores a trailing newline", [] {
        expect(blogin::text::split_lines("one\ntwo\n").size()).to_eq(std::size_t{2});
      });
    });

    spec::context("words", [] {
      spec::it("counts whitespace-separated runs", [] {
        expect(blogin::text::word_count("one two three")).to_eq(std::size_t{3});
      });

      spec::it("ignores repeated whitespace", [] {
        expect(blogin::text::word_count("one   two")).to_eq(std::size_t{2});
      });

      spec::it("ignores leading and trailing whitespace", [] {
        expect(blogin::text::word_count("  one  ")).to_eq(std::size_t{1});
      });

      spec::it("counts nothing in an empty string", [] {
        expect(blogin::text::word_count("")).to_eq(std::size_t{0});
      });

      spec::it("counts nothing in whitespace alone", [] {
        expect(blogin::text::word_count(" \t\n ")).to_eq(std::size_t{0});
      });

      spec::it("treats a newline as a separator", [] {
        expect(blogin::text::word_count("one\ntwo")).to_eq(std::size_t{2});
      });
    });

    spec::context("case folding", [] {
      spec::it("lowers ascii", [] { expect(blogin::text::to_lower_ascii("AbC")).to_eq("abc"); });

      spec::it("raises ascii", [] { expect(blogin::text::to_upper_ascii("AbC")).to_eq("ABC"); });

      // Deliberate: folding the rest of Unicode needs tables this project does
      // not carry, and every caller here is matching ASCII keywords or slugging.
      spec::it("leaves non-ascii untouched", [] {
        expect(blogin::text::to_lower_ascii("CAFÉ")).to_eq("cafÉ");
      });

      spec::it("leaves digits and punctuation alone", [] {
        expect(blogin::text::to_lower_ascii("A1-B")).to_eq("a1-b");
      });
    });

    // JSON numbers, YAML scalars, a post's `order`, and a number written in a
    // template all arrive here. `std::from_chars` over a double would be the
    // obvious way to read one, and libc++ marks it unavailable below macOS 26,
    // which is the reason it exists.
    spec::context("reading a number", [] {
      const auto read = [](std::string_view text) {
        return blogin::text::to_double(text).value_or(-1.0);
      };

      spec::it("reads a whole number", [=] { expect(read("42")).to_eq(42.0); });

      spec::it("reads a decimal", [=] { expect(read("2.5")).to_eq(2.5); });

      spec::it("reads a negative number", [=] { expect(read("-0.5")).to_eq(-0.5); });

      spec::it("reads an exponent", [=] { expect(read("1e3")).to_eq(1000.0); });

      spec::it("reads a decimal point as a point", [=] { expect(read("1.5")).to_eq(1.5); });

      spec::it("refuses trailing characters", [] {
        expect(blogin::text::to_double("1.5kg").has_value()).to_be_false();
      });

      spec::it("refuses leading characters", [] {
        expect(blogin::text::to_double("kg1.5").has_value()).to_be_false();
      });

      spec::it("refuses an empty string", [] {
        expect(blogin::text::to_double("").has_value()).to_be_false();
      });

      spec::it("refuses a number too large to hold", [] {
        expect(blogin::text::to_double("1e400").has_value()).to_be_false();
      });
    });
  });
}
