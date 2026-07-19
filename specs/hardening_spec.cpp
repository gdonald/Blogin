#include <version>

#include "support/spec.h"

using spec::expect;

namespace {

// Whether the standard library in this build checks a subscript against the
// container's size. The two implementations spell it differently, and either
// one without its define compiles the checks out.
#ifdef _LIBCPP_VERSION
#  if defined(_LIBCPP_HARDENING_MODE) && defined(_LIBCPP_HARDENING_MODE_FAST)
constexpr bool containers_are_bounds_checked = _LIBCPP_HARDENING_MODE >= _LIBCPP_HARDENING_MODE_FAST;
#  else
constexpr bool containers_are_bounds_checked = false;
#  endif
#elifdef __GLIBCXX__
#  ifdef _GLIBCXX_ASSERTIONS
constexpr bool containers_are_bounds_checked = true;
#  else
constexpr bool containers_are_bounds_checked = false;
#  endif
#else
constexpr bool containers_are_bounds_checked = false;
#endif

}  // namespace

SPEC {
  spec::describe("the build", [] {
    // Losing this define is silent in a way the rest of the suite cannot catch.
    // An out-of-range subscript stops trapping and goes back to returning
    // whatever sits after the container, so every spec still passes while the
    // shipped binary reads past its buffers.
    spec::it("bounds-checks standard library subscripting", [] {
      expect(containers_are_bounds_checked).to_be_true();
    });
  });
}
