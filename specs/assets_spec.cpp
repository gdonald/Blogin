#include <map>
#include <random>
#include <string>
#include <vector>

#include "assets.h"
#include "support/spec.h"

using spec::expect;

SPEC {
  spec::describe("minifying css", [] {
    spec::it("removes a comment", [] {
      expect(blogin::assets::minify_css("a { color: red; } /* note */")).to_eq("a{color:red}");
    });

    spec::it("collapses a run of whitespace", [] {
      expect(blogin::assets::minify_css("a\n\n  b   {  color :  red  }")).to_eq("a b{color:red}");
    });

    spec::it("drops the semicolon before a closing brace", [] {
      expect(blogin::assets::minify_css("a { color: red; }")).to_eq("a{color:red}");
    });

    // The trailing space is dropped where it falls, so a rule ending in one
    // does not carry it into the output.
    spec::it("drops a space before a closing brace", [] {
      expect(blogin::assets::minify_css("a { color: red ; }")).to_eq("a{color:red}");
    });

    spec::it("drops a space at the very end", [] {
      expect(blogin::assets::minify_css("a{color:red}   ")).to_eq("a{color:red}");
    });

    // The minifier writes a pending space only when content follows it, so no
    // output can end in one. This is the property that lets it hold.
    spec::it("never ends the output with a space", [] {
      const std::string alphabet = "ab{}; \n\t:/*,()#.-0";

      std::mt19937 engine(7);
      std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
      std::uniform_int_distribution<int> length(0, 40);

      std::size_t trailing = 0;

      for (int run = 0; run < 2000; ++run) {
        std::string input;

        for (int index = 0, size = length(engine); index < size; ++index) {
          input += alphabet[pick(engine)];
        }

        for (const std::string& out : {blogin::assets::minify_css(input), blogin::assets::minify_js(input)}) {
          if (!out.empty() && out.back() == ' ') {
            ++trailing;
          }
        }
      }

      expect(trailing).to_eq(std::size_t{0});
    });

    spec::it("keeps the space a descendant selector depends on", [] {
      expect(blogin::assets::minify_css(".card .title { margin: 0 }")).to_eq(".card .title{margin:0}");
    });

    spec::it("keeps the space between the parts of a shorthand value", [] {
      expect(blogin::assets::minify_css("a { margin: 0 auto 4px }")).to_eq("a{margin:0 auto 4px}");
    });

    spec::it("removes the space around a child combinator", [] {
      expect(blogin::assets::minify_css("ul > li { color: red }")).to_eq("ul>li{color:red}");
    });

    // Punctuation inside a string is content. A minifier that treated it as
    // syntax would silently change what the page displays.
    spec::it("leaves a semicolon inside a string alone", [] {
      expect(blogin::assets::minify_css("a::after { content: 'a; b' }")).to_eq("a::after{content:'a; b'}");
    });

    spec::it("leaves what looks like a comment inside a string alone", [] {
      expect(blogin::assets::minify_css("a::after { content: '/* x */' }"))
        .to_eq("a::after{content:'/* x */'}");
    });

    spec::it("leaves a url alone", [] {
      expect(blogin::assets::minify_css("a { background: url(/assets/img/x.png) }"))
        .to_eq("a{background:url(/assets/img/x.png)}");
    });

    spec::it("yields nothing for a stylesheet that is only a comment", [] {
      expect(blogin::assets::minify_css("/* everything */")).to_eq("");
    });
  });

  spec::describe("minifying javascript", [] {
    spec::it("removes indentation", [] {
      expect(blogin::assets::minify_js("function a() {\n  return 1\n}"))
        .to_eq("function a() {\nreturn 1\n}");
    });

    spec::it("removes a blank line", [] {
      expect(blogin::assets::minify_js("a\n\n\nb")).to_eq("a\nb");
    });

    spec::it("removes a whole-line comment", [] {
      expect(blogin::assets::minify_js("// a note\nvalue")).to_eq("value");
    });

    // Joining lines is what breaks minifiers without a parser, because a
    // semicolon the language would have inserted stops being inserted.
    spec::it("keeps every remaining line on its own line", [] {
      expect(blogin::assets::minify_js("return\n1")).to_eq("return\n1");
    });

    spec::it("keeps an escaped character inside a string", [] {
      expect(blogin::assets::minify_css(R"(a { content: "\\"" })")).to_contain(R"(\\")");
    });

    spec::it("leaves a trailing comment on a line of code alone", [] {
      expect(blogin::assets::minify_js("  value // why\n")).to_eq("value // why");
    });
  });

  spec::describe("fingerprinting", [] {
    spec::it("puts the hash before the extension", [] {
      expect(blogin::assets::fingerprint_name("styles.css", "abcd1234")).to_eq("styles.abcd1234.css");
    });

    spec::it("appends the hash when there is no extension", [] {
      expect(blogin::assets::fingerprint_name("CNAME", "abcd1234")).to_eq("CNAME.abcd1234");
    });

    spec::it("keeps the last extension of a name with several", [] {
      expect(blogin::assets::fingerprint_name("a.min.js", "abcd1234")).to_eq("a.min.abcd1234.js");
    });

    spec::it("accepts a stylesheet", [] {
      expect(blogin::assets::is_fingerprintable("a/styles.css")).to_be_true();
    });

    spec::it("accepts an image whose extension is capitalised", [] {
      expect(blogin::assets::is_fingerprintable("a/photo.PNG")).to_be_true();
    });

    spec::it("refuses a file with no extension", [] {
      expect(blogin::assets::is_fingerprintable("a/CNAME")).to_be_false();
    });

    spec::it("refuses a page", [] {
      expect(blogin::assets::is_fingerprintable("a/index.html")).to_be_false();
    });
  });

  spec::describe("rewriting asset references", [] {
    const std::map<std::string, std::string> manifest{{"/assets/css/a.css", "/assets/css/a.ffff.css"}};

    spec::it("rewrites a quoted reference", [=] {
      expect(blogin::assets::rewrite_refs("<link href=\"/assets/css/a.css\">", manifest))
        .to_eq("<link href=\"/assets/css/a.ffff.css\">");
    });

    spec::it("rewrites a reference inside a css url", [=] {
      expect(blogin::assets::rewrite_refs("a{background:url(/assets/css/a.css)}", manifest))
        .to_eq("a{background:url(/assets/css/a.ffff.css)}");
    });

    spec::it("rewrites every occurrence", [=] {
      expect(blogin::assets::rewrite_refs("'/assets/css/a.css' '/assets/css/a.css'", manifest))
        .to_eq("'/assets/css/a.ffff.css' '/assets/css/a.ffff.css'");
    });

    // A path that only starts with one the manifest names is a different file.
    spec::it("leaves a longer path that shares a prefix alone", [=] {
      expect(blogin::assets::rewrite_refs("\"/assets/css/a.css.map\"", manifest))
        .to_eq("\"/assets/css/a.css.map\"");
    });

    spec::it("leaves a url the manifest does not name alone", [=] {
      expect(blogin::assets::rewrite_refs("\"/assets/css/b.css\"", manifest))
        .to_eq("\"/assets/css/b.css\"");
    });

    spec::it("leaves the text alone when the manifest is empty", [] {
      expect(blogin::assets::rewrite_refs("\"/assets/css/a.css\"", {}))
        .to_eq("\"/assets/css/a.css\"");
    });
  });

  spec::describe("responsive images", [] {
    spec::it("names a variant by its width", [] {
      expect(blogin::assets::variant_name("photo.jpg", 640)).to_eq("photo-640.jpg");
    });

    spec::it("accepts a raster image", [] {
      expect(blogin::assets::is_raster("a/photo.jpg")).to_be_true();
    });

    // Resizing a vector image gains nothing, though it is still fingerprinted.
    spec::it("refuses a vector image", [] {
      expect(blogin::assets::is_raster("a/logo.svg")).to_be_false();
    });

    spec::it("lists each variant and then the original", [] {
      const std::vector<blogin::assets::Variant> variants{{320, "/a-320.jpg"}, {640, "/a-640.jpg"}};

      expect(blogin::assets::srcset_value("/a.jpg", 1280, variants))
        .to_eq("/a-320.jpg 320w, /a-640.jpg 640w, /a.jpg 1280w");
    });

    spec::it("lists only the original when nothing was resized", [] {
      expect(blogin::assets::srcset_value("/a.jpg", 1280, {})).to_eq("/a.jpg 1280w");
    });

    spec::it("adds a srcset beside the src it names", [] {
      const std::map<std::string, std::string> srcsets{{"/a.jpg", "/a-320.jpg 320w, /a.jpg 640w"}};

      expect(blogin::assets::add_srcset("<img src=\"/a.jpg\">", srcsets))
        .to_eq(R"(<img src="/a.jpg" srcset="/a-320.jpg 320w, /a.jpg 640w">)");
    });

    spec::it("leaves a src it does not name alone", [] {
      const std::map<std::string, std::string> srcsets{{"/a.jpg", "/a-320.jpg 320w"}};

      expect(blogin::assets::add_srcset("<img src=\"/b.jpg\">", srcsets)).to_eq("<img src=\"/b.jpg\">");
    });

    spec::it("adds a srcset to every occurrence", [] {
      const std::map<std::string, std::string> srcsets{{"/a.jpg", "/a-320.jpg 320w"}};

      expect(blogin::assets::add_srcset(R"(<img src="/a.jpg"><img src="/a.jpg">)", srcsets))
        .to_eq(R"(<img src="/a.jpg" srcset="/a-320.jpg 320w"><img src="/a.jpg" srcset="/a-320.jpg 320w">)");
    });

    spec::it("leaves the page alone when nothing was resized", [] {
      expect(blogin::assets::add_srcset("<img src=\"/a.jpg\">", {})).to_eq("<img src=\"/a.jpg\">");
    });

    spec::it("names a variant of a file with no extension", [] {
      expect(blogin::assets::variant_name("photo", 640)).to_eq("photo-640");
    });

    spec::it("leaves a src whose quote never closes alone", [] {
      const std::map<std::string, std::string> srcsets{{"/a.jpg", "/a-320.jpg 320w"}};

      expect(blogin::assets::add_srcset("<img src=\"/a.jpg", srcsets)).to_eq("<img src=\"/a.jpg");
    });
  });
}

SPEC {
  spec::describe("measuring an image", [] {
    spec::it("reads no width when the tool cannot be run", [] {
      expect(blogin::assets::image_width("any.png", "definitely-not-a-resizer")).to_eq(0);
    });

    spec::it("reads no width for a file that is not there", [] {
      const std::string tool = blogin::assets::resizer();

      if (tool.empty()) {
        spec::pending("no image resizer installed");
      }

      expect(blogin::assets::image_width("no-such-image.png", tool)).to_eq(0);
    });

    // A quote in a filename would end the shell's argument early.
    spec::it("reads no width for a name carrying a quote", [] {
      const std::string tool = blogin::assets::resizer();

      if (tool.empty()) {
        spec::pending("no image resizer installed");
      }

      expect(blogin::assets::image_width("it's not here.png", tool)).to_eq(0);
    });
  });
}
