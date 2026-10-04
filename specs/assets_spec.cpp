#include <format>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "assets.h"
#include "support/spec.h"

using spec::expect;

namespace {

std::string big_endian(unsigned value, int bytes) {
  std::string out;

  for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) {
    out += static_cast<char>((value >> static_cast<unsigned>(shift)) & 0xFFU);
  }

  return out;
}

std::string little_endian(unsigned value, int bytes) {
  std::string out;

  for (int index = 0; index < bytes; ++index) {
    out += static_cast<char>((value >> static_cast<unsigned>(index * 8)) & 0xFFU);
  }

  return out;
}

// Each header carries only the bytes the size is read from, so a spec says
// which field it depends on.
std::string png_header(unsigned width, unsigned height) {
  return std::string("\x89PNG\r\n\x1A\n", 8) + big_endian(13, 4) + "IHDR" + big_endian(width, 4) +
         big_endian(height, 4);
}

std::string gif_header(unsigned width, unsigned height) {
  return "GIF89a" + little_endian(width, 2) + little_endian(height, 2);
}

// An APP0 segment before the frame header, as a camera or editor writes one.
std::string jpeg_header(unsigned width, unsigned height, unsigned frame_marker = 0xC0) {
  return std::string("\xFF\xD8", 2) + std::string("\xFF\xE0", 2) + big_endian(16, 2) + std::string(14, 'j') +
         "\xFF" + static_cast<char>(frame_marker) + big_endian(17, 2) + "\x08" + big_endian(height, 2) +
         big_endian(width, 2);
}

std::string webp_header(std::string_view chunk, std::string_view payload) {
  return "RIFF" + little_endian(0, 4) + "WEBP" + std::string(chunk) + little_endian(0, 4) + std::string(payload) +
         std::string(16, '\0');
}

std::string webp_lossy(unsigned width, unsigned height) {
  return webp_header("VP8 ", std::string(3, '\0') + "\x9D\x01\x2A" + little_endian(width, 2) +
                               little_endian(height, 2));
}

std::string webp_lossless(unsigned width, unsigned height) {
  return webp_header("VP8L", std::string(1, '\x2F') + little_endian((width - 1) | ((height - 1) << 14U), 4));
}

std::string webp_extended(unsigned width, unsigned height) {
  return webp_header("VP8X", std::string(4, '\0') + little_endian(width - 1, 3) + little_endian(height - 1, 3));
}

std::string size_of(std::string_view bytes) {
  const std::optional<blogin::assets::ImageSize> size = blogin::assets::image_size(bytes);

  return size ? std::format("{}x{}", size->width, size->height) : "none";
}

}  // namespace

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

SPEC {
  spec::describe("reading an image's size from its header", [] {
    spec::it("reads a png", [] { expect(size_of(png_header(1200, 627))).to_eq("1200x627"); });

    spec::it("reads a gif", [] { expect(size_of(gif_header(640, 480))).to_eq("640x480"); });

    spec::it("reads a jpeg past the segments before its frame header", [] {
      expect(size_of(jpeg_header(1200, 627))).to_eq("1200x627");
    });

    spec::it("reads a progressive jpeg", [] { expect(size_of(jpeg_header(800, 600, 0xC2))).to_eq("800x600"); });

    // DHT shares the frame header's marker range and carries no size.
    spec::it("does not take a huffman table for a jpeg frame header", [] {
      expect(size_of(jpeg_header(800, 600, 0xC4))).to_eq("none");
    });

    spec::it("steps over fill bytes between jpeg segments", [] {
      std::string bytes = jpeg_header(800, 600);
      bytes.insert(20, "\xFF");

      expect(size_of(bytes)).to_eq("800x600");
    });

    spec::it("reads a lossy webp", [] { expect(size_of(webp_lossy(1200, 627))).to_eq("1200x627"); });

    spec::it("reads a lossless webp", [] { expect(size_of(webp_lossless(1200, 627))).to_eq("1200x627"); });

    spec::it("reads an extended webp", [] { expect(size_of(webp_extended(1200, 627))).to_eq("1200x627"); });

    spec::it("reads nothing from a webp chunk it does not know", [] {
      expect(size_of(webp_header("ALPH", std::string(10, '\0')))).to_eq("none");
    });

    spec::it("reads nothing from an svg", [] {
      expect(size_of("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'/>")).to_eq("none");
    });

    spec::it("reads nothing from a truncated png", [] {
      expect(size_of(png_header(10, 10).substr(0, 20))).to_eq("none");
    });

    spec::it("reads nothing from a png whose first chunk is not its header", [] {
      std::string bytes = png_header(10, 10);
      bytes.replace(12, 4, "IDAT");

      expect(size_of(bytes)).to_eq("none");
    });

    spec::it("reads nothing from a truncated gif", [] {
      expect(size_of(gif_header(10, 10).substr(0, 8))).to_eq("none");
    });

    spec::it("reads nothing from a truncated jpeg frame header", [] {
      const std::string bytes = jpeg_header(800, 600);

      expect(size_of(bytes.substr(0, bytes.size() - 2))).to_eq("none");
    });

    spec::it("reads nothing from a jpeg that ends before its frame header", [] {
      expect(size_of(jpeg_header(800, 600).substr(0, 22))).to_eq("none");
    });

    spec::it("reads nothing from a jpeg whose segments lose their markers", [] {
      std::string bytes = jpeg_header(800, 600);
      bytes[20] = 'x';

      expect(size_of(bytes)).to_eq("none");
    });

    spec::it("reads nothing from a jpeg segment too short to hold its own length", [] {
      std::string bytes = jpeg_header(800, 600);
      bytes.replace(4, 2, big_endian(1, 2));

      expect(size_of(bytes)).to_eq("none");
    });

    spec::it("reads nothing from a truncated webp", [] {
      expect(size_of(webp_lossy(10, 10).substr(0, 24))).to_eq("none");
    });

    spec::it("reads nothing from a size of zero", [] { expect(size_of(gif_header(0, 10))).to_eq("none"); });

    spec::it("reads nothing from a size too large to be an image", [] {
      expect(size_of(png_header(0x80000000U, 10))).to_eq("none");
    });

    spec::it("reads nothing from an empty file", [] { expect(size_of("")).to_eq("none"); });
  });
}
