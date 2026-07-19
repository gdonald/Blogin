#pragma once

#include <string_view>

namespace blogin::style {

// The stylesheet Blogin emits for the markup it generates: highlighting colours,
// heading anchors, pagination, post navigation, definition lists, and the light
// and dark theme toggle. A site's own stylesheet loads after it.
std::string_view content_css();

}  // namespace blogin::style
