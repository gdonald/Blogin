#include "framework.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace blogin {
namespace {

struct Slot {
  std::string_view slot;
  std::string_view value;
};

struct Profile {
  std::string_view name;
  std::string_view stylesheet;
  std::string_view script;
  std::vector<Slot> slots;
};

const std::vector<Profile>& profiles() {
  // Every slot the renderer and the view ask for is answered here, or left out
  // where the framework styles that element without a class. Which of the two
  // it is comes from each framework's own documentation, and specs/theme_spec
  // asserts every entry below.
  static const std::vector<Profile> known{
    // Classless by intent. Nothing is added anywhere.
    {"none", "", "", {}},

    // Bootstrap styles bare headings, lists, definition lists, and code, so
    // those slots stay empty. Everything it does class is here.
    {"bootstrap5",
     "https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css",
     "https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js",
     {
       {"table", "table"},
       {"blockquote", "blockquote"},
       {"image", "img-fluid"},
       {"pagination-list", "pagination"},
       {"pagination-item", "page-item"},
       {"pagination-link", "page-link"},
       {"pagination-active-item", "active"},
       {"nav", "nav"},
       {"container", "container"},
       {"tag", "badge text-bg-secondary"},
       {"post-nav-button", "btn btn-primary"},
     }},

    // Pico styles bare semantic HTML, which is the whole of its design, so the
    // only class it defines is the one that bounds page width.
    {"pico",
     "https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css",
     "",
     {
       {"container", "container"},
     }},

    // Bulma does not style bare typography. Headings, lists, blockquotes, and
    // tables are styled only inside `.content`, which is what the `article`
    // slot wraps the rendered body in.
    {"bulma",
     "https://cdn.jsdelivr.net/npm/bulma@1/css/bulma.min.css",
     "",
     {
       {"article", "content"},
       {"table", "table"},
       {"image", "image"},
       {"pagination-nav", "pagination"},
       {"pagination-list", "pagination-list"},
       {"pagination-link", "pagination-link"},
       {"pagination-active-link", "is-current"},
       {"nav", "navbar"},
       {"container", "container"},
       {"tag", "tag"},
       {"post-nav-button", "button"},
     }},
  };

  return known;
}

const Profile* find(std::string_view name) {
  const auto found = std::find_if(profiles().begin(), profiles().end(),
                                  [&](const Profile& profile) { return profile.name == name; });

  return found == profiles().end() ? nullptr : &*found;
}

}  // namespace

bool Framework::known(std::string_view name) {
  return find(name) != nullptr;
}

std::vector<std::string_view> Framework::names() {
  std::vector<std::string_view> out;

  for (const Profile& profile : profiles()) {
    out.push_back(profile.name);
  }

  return out;
}

Framework Framework::profile(std::string_view name) {
  const Profile* found = find(name);

  if (found == nullptr) {
    std::string known_names;

    for (const std::string_view candidate : names()) {
      if (!known_names.empty()) {
        known_names += ", ";
      }

      known_names += candidate;
    }

    throw std::runtime_error("unknown css-framework '" + std::string(name) + "' (known: " + known_names + ")");
  }

  Framework framework;
  framework.name_ = found->name;
  framework.stylesheet_ = found->stylesheet;
  framework.script_ = found->script;

  return framework;
}

std::string_view Framework::class_for(std::string_view slot) const {
  const Profile* found = find(name_);

  if (found == nullptr) {
    return {};
  }

  const auto match = std::find_if(found->slots.begin(), found->slots.end(),
                                  [&](const Slot& entry) { return entry.slot == slot; });

  return match == found->slots.end() ? std::string_view{} : match->value;
}

}  // namespace blogin
