#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>

#include "http.h"
#include "require.h"

namespace {

// A tree to resolve against, built once. Serving is the one place blogin turns
// bytes off a socket into a filesystem path, so the containment check is what
// stands between a request and the rest of the disk.
//
// The symlink matters as much as the files do: a check written against the
// spelling of a path would pass "escape" and hand back what it points at, and
// only a check on the resolved path rejects it.
const std::filesystem::path& fixture_root() {
  static const std::filesystem::path root = [] {
    const std::filesystem::path base =
      std::filesystem::temp_directory_path() /
      ("blogin-fuzz-" + std::to_string(::getpid()));

    const std::filesystem::path served = base / "site";

    std::filesystem::create_directories(served / "sub");

    for (const std::filesystem::path& file :
         {served / "index.html", served / "page.html", served / "sub" / "index.html"}) {
      std::ofstream(file) << "<!doctype html>\n";
    }

    std::ofstream(base / "outside.txt") << "not served\n";

    std::error_code error;
    std::filesystem::create_symlink(base / "outside.txt", served / "escape", error);
    std::filesystem::create_symlink(base, served / "up", error);

    return served;
  }();

  return root;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string url_path(reinterpret_cast<const char*>(data), size);

  const std::filesystem::path& root = fixture_root();

  const std::optional<std::filesystem::path> resolved =
    blogin::http::resolve_file(url_path, root);

  if (!resolved) {
    return 0;
  }

  // Whatever the request spelled, what comes back has to be a real file inside
  // the tree being served. Anything else is the bytes on the socket choosing
  // which file on the machine to read.
  std::error_code error;

  const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);
  const auto relation = resolved->lexically_relative(base);

  FUZZ_REQUIRE(!error);
  FUZZ_REQUIRE(!relation.empty());
  FUZZ_REQUIRE(*relation.begin() != "..");
  FUZZ_REQUIRE(!resolved->is_relative());
  FUZZ_REQUIRE(std::filesystem::is_regular_file(*resolved, error));

  return 0;
}
