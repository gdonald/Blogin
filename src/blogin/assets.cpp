#include "assets.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <format>

#include "text.h"

namespace blogin::assets {
namespace {

bool has_extension(const std::filesystem::path& file, std::initializer_list<std::string_view> allowed) {
  const std::string extension = text::to_lower_ascii(file.extension().string());

  if (extension.empty()) {
    return false;
  }

  const std::string_view without_dot(extension.begin() + 1, extension.end());

  return std::find(allowed.begin(), allowed.end(), without_dot) != allowed.end();
}

bool is_space(char character) {
  return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
         character == '\f';
}

// Structural punctuation, where the surrounding whitespace carries no meaning.
bool is_structural(char character) {
  return character == '{' || character == '}' || character == ':' || character == ';' ||
         character == ',' || character == '>';
}

std::string quote(const std::filesystem::path& path) {
  std::string out = "'";

  for (const char character : path.string()) {
    if (character == '\'') {
      out += "'\\''";
    } else {
      out += character;
    }
  }

  out += "'";

  return out;
}

// The command's standard output, and whether it succeeded. Standard error goes
// to the null device, since a resizer complaining about one file should not
// land in the middle of a build's output.
std::pair<bool, std::string> run_quiet(const std::string& command) {
  // NOLINTNEXTLINE(bugprone-command-processor) a shell is what runs the resizer
  std::FILE* pipe = ::popen((command + " 2>/dev/null").c_str(), "r");

  if (pipe == nullptr) {
    return {false, {}};
  }

  std::string out;
  std::array<char, 256> buffer{};

  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    out += buffer.data();
  }

  return {::pclose(pipe) == 0, out};
}

bool tool_available(std::string_view name) {
  const char* path = std::getenv("PATH");

  if (path == nullptr) {
    return false;
  }

  std::error_code error;

  for (const std::string_view directory : text::split(path, ':')) {
    if (directory.empty()) {
      continue;
    }

    const std::filesystem::path candidate = std::filesystem::path(directory) / name;

    if (std::filesystem::is_regular_file(candidate, error)) {
      const auto permissions = std::filesystem::status(candidate, error).permissions();

      if (!error && (permissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
        return true;
      }
    }
  }

  return false;
}

std::string stem_of(std::string_view filename, std::string_view& extension) {
  const auto dot = filename.rfind('.');

  if (dot == std::string_view::npos || dot == 0) {
    extension = {};

    return std::string(filename);
  }

  extension = filename.substr(dot + 1);

  return std::string(filename.substr(0, dot));
}

}  // namespace

std::string minify_css(std::string_view css) {
  std::string out;
  out.reserve(css.size());

  bool pending_space = false;

  // Whitespace after structural punctuation carries no meaning either, so a
  // space is only ever emitted between two things that need separating.
  const auto emit_pending = [&] {
    if (pending_space && !out.empty() && !is_structural(out.back())) {
      out += ' ';
    }

    pending_space = false;
  };

  for (std::size_t index = 0; index < css.size();) {
    const char character = css[index];

    if (character == '/' && index + 1 < css.size() && css[index + 1] == '*') {
      const auto closing = css.find("*/", index + 2);

      index = closing == std::string_view::npos ? css.size() : closing + 2;
      pending_space = true;

      continue;
    }

    // A string is content. Whatever punctuation it holds stays exactly as it is.
    if (character == '"' || character == '\'') {
      emit_pending();

      const char opening = character;
      out += opening;
      ++index;

      while (index < css.size() && css[index] != opening) {
        if (css[index] == '\\' && index + 1 < css.size()) {
          out += css[index];
          ++index;
        }

        out += css[index];
        ++index;
      }

      if (index < css.size()) {
        out += opening;
        ++index;
      }

      continue;
    }

    if (is_space(character)) {
      pending_space = true;
      ++index;

      continue;
    }

    if (is_structural(character)) {
      // A semicolon before a closing brace ends nothing.
      if (character == '}' && !out.empty() && out.back() == ';') {
        out.pop_back();
      }

      out += character;
      pending_space = false;
      ++index;

      continue;
    }

    emit_pending();

    out += character;
    ++index;
  }

  return out;
}

std::string minify_js(std::string_view js) {
  std::string out;

  for (const std::string_view line : text::split(js, '\n')) {
    const std::string_view trimmed = text::trim(line);

    if (trimmed.empty() || trimmed.starts_with("//")) {
      continue;
    }

    if (!out.empty()) {
      out += '\n';
    }

    out += trimmed;
  }

  return out;
}

bool is_fingerprintable(const std::filesystem::path& file) {
  return has_extension(file, {"css", "js", "png", "jpg", "jpeg", "gif", "webp", "svg", "ico"});
}

std::string fingerprint_name(std::string_view filename, std::string_view hash) {
  std::string_view extension;
  const std::string stem = stem_of(filename, extension);

  if (extension.empty()) {
    return std::format("{}.{}", filename, hash);
  }

  return std::format("{}.{}.{}", stem, hash, extension);
}

std::string rewrite_refs(std::string_view text, const std::map<std::string, std::string>& manifest) {
  if (manifest.empty()) {
    return std::string(text);
  }

  const auto is_delimiter = [](char character) {
    return character == '"' || character == '\'' || character == '(' || character == ')' ||
           character == ',' || is_space(character);
  };

  std::string out;
  out.reserve(text.size());

  std::size_t index = 0;

  while (index < text.size()) {
    const char character = text[index];

    out += character;
    ++index;

    // Only a delimited absolute path can be a url, which keeps the lookup off
    // ordinary prose.
    if (!is_delimiter(character) || index >= text.size() || text[index] != '/') {
      continue;
    }

    std::size_t end = index;

    while (end < text.size() && !is_delimiter(text[end])) {
      ++end;
    }

    const std::string_view candidate = text.substr(index, end - index);
    const auto found = manifest.find(std::string(candidate));

    if (found == manifest.end()) {
      continue;
    }

    out += found->second;
    index = end;
  }

  return out;
}

bool is_raster(const std::filesystem::path& file) {
  return has_extension(file, {"png", "jpg", "jpeg", "gif", "webp"});
}

std::string variant_name(std::string_view filename, int width) {
  std::string_view extension;
  const std::string stem = stem_of(filename, extension);

  if (extension.empty()) {
    return std::format("{}-{}", filename, width);
  }

  return std::format("{}-{}.{}", stem, width, extension);
}

std::string srcset_value(std::string_view original_url, int original_width,
                         const std::vector<Variant>& variants) {
  std::string out;

  for (const Variant& variant : variants) {
    out += std::format("{} {}w, ", variant.url, variant.width);
  }

  out += std::format("{} {}w", original_url, original_width);

  return out;
}

std::string add_srcset(std::string_view html, const std::map<std::string, std::string>& srcsets) {
  if (srcsets.empty()) {
    return std::string(html);
  }

  std::string out;
  out.reserve(html.size());

  constexpr std::string_view opening = "src=\"";

  std::size_t index = 0;

  while (index < html.size()) {
    if (html.compare(index, opening.size(), opening) != 0) {
      out += html[index];
      ++index;

      continue;
    }

    const auto closing = html.find('"', index + opening.size());

    if (closing == std::string_view::npos) {
      out += html[index];
      ++index;

      continue;
    }

    const std::string_view url = html.substr(index + opening.size(), closing - index - opening.size());
    const auto found = srcsets.find(std::string(url));

    out += opening;
    out += url;
    out += '"';

    if (found != srcsets.end()) {
      out += " srcset=\"";
      out += found->second;
      out += '"';
    }

    index = closing + 1;
  }

  return out;
}

std::string resizer() {
  for (const std::string_view tool : {"magick", "convert", "sips"}) {
    if (tool_available(tool)) {
      return std::string(tool);
    }
  }

  return {};
}

int image_width(const std::filesystem::path& file, std::string_view tool) {
  const std::string quoted = quote(file);

  const std::string command =
    tool == "sips"     ? std::format("sips -g pixelWidth {}", quoted)
    : tool == "magick" ? std::format("magick identify -format %w {}", quoted)
                       : std::format("identify -format %w {}", quoted);

  const auto [ok, out] = run_quiet(command);

  if (!ok) {
    return 0;
  }

  // sips echoes the file's path before the answer, and a path can contain
  // digits, so the number is read from after the label, not from the
  // start of the output.
  std::size_t start = 0;

  if (const auto label = out.find("pixelWidth"); label != std::string::npos) {
    start = label + std::string_view("pixelWidth").size();
  }

  std::string digits;

  for (std::size_t index = start; index < out.size(); ++index) {
    const char character = out[index];

    if (character >= '0' && character <= '9') {
      digits += character;
    } else if (!digits.empty()) {
      break;
    }
  }

  int pixels = 0;

  if (std::from_chars(digits.data(), digits.data() + digits.size(), pixels).ec != std::errc{}) {
    return 0;
  }

  return pixels;
}

bool resize(const std::filesystem::path& source, const std::filesystem::path& destination, int width,
            std::string_view tool) {
  const std::string from = quote(source);
  const std::string to = quote(destination);

  const std::string command =
    tool == "sips"     ? std::format("sips --resampleWidth {} {} --out {}", width, from, to)
    : tool == "magick" ? std::format("magick {} -resize {}x {}", from, width, to)
                       : std::format("convert {} -resize {}x {}", from, width, to);

  return run_quiet(command).first;
}

}  // namespace blogin::assets
