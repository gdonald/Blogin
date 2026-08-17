#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace blogin::assets {

// Comments out, whitespace collapsed, and the space around structural
// punctuation removed. Strings and url() are copied through untouched, since a
// semicolon or a /* inside one is content, not syntax.
std::string minify_css(std::string_view css);

// Line-oriented and safe against automatic semicolon insertion: blank lines,
// whole-line comments, and indentation go, and nothing inside a line is
// touched. A minifier that rewrote expressions would need a JavaScript parser
// to be correct, and the bytes it would save do not pay for one.
std::string minify_js(std::string_view js);

// Assets that take a content-addressed name. Everything else keeps the name it
// was written with.
bool is_fingerprintable(const std::filesystem::path& file);

// "styles.css" and a hash become "styles.<hash>.css", so the extension still
// decides the content type.
std::string fingerprint_name(std::string_view filename, std::string_view hash);

// Replaces each url in `text` that the manifest names, matching only paths that
// appear as a delimited token: quoted, inside a CSS url(), or as a srcset entry.
//
// One pass over the text, so the cost follows the length of the page rather
// than the length of the page times the number of assets.
std::string rewrite_refs(std::string_view text, const std::map<std::string, std::string>& manifest);

// Images that can be resized. SVG is fingerprintable but not raster.
bool is_raster(const std::filesystem::path& file);

// "photo.jpg" at 640 becomes "photo-640.jpg".
std::string variant_name(std::string_view filename, int width);

struct Variant {
  int width = 0;
  std::string url;
};

// Each variant by width, then the original at its own width.
std::string srcset_value(std::string_view original_url, int original_width,
                         const std::vector<Variant>& variants);

// Adds a srcset beside each src the map names. A page written before the
// variants existed says only which image it wants. This says which sizes of it
// are available.
std::string add_srcset(std::string_view html, const std::map<std::string, std::string>& srcsets);

// The first available image resizer, preferring ImageMagick for the same
// results on every platform, then macOS sips. Empty when none is installed.
std::string resizer();

// Zero when the tool cannot read the file.
int image_width(const std::filesystem::path& file, std::string_view tool);

bool resize(const std::filesystem::path& source, const std::filesystem::path& destination, int width,
            std::string_view tool);

}  // namespace blogin::assets
