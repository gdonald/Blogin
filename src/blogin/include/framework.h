#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace blogin {

// A CSS framework adds classes to the same semantic HTML. The renderer asks for
// a slot by name and writes whatever the profile returns, so no framework is
// named anywhere in the rendering code.
class Framework {
public:
  static bool known(std::string_view name);

  static std::vector<std::string_view> names();

  // Throws for a name that is not one of the known profiles, since a typo in
  // configuration should stop the build rather than silently style nothing.
  static Framework profile(std::string_view name);

  std::string_view name() const { return name_; }

  std::string_view class_for(std::string_view slot) const;

  std::string_view stylesheet() const { return stylesheet_; }

  std::string_view script() const { return script_; }

private:
  std::string_view name_ = "none";
  std::string_view stylesheet_;
  std::string_view script_;
};

}  // namespace blogin
