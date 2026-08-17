#include "view.h"

namespace blogin::view {

// Both of these are assets, not logic. The script has to run before
// first paint to avoid a flash of the wrong theme, so it is inlined verbatim
// instead of assembled.
std::string_view theme_script() {
  return R"JS(    <script>
    (function () {
      var KEY = 'blogin-theme', root = document.documentElement;
      function apply(m) { root.setAttribute('data-theme', m); root.setAttribute('data-bs-theme', m); }
      function pref() {
        try { var s = localStorage.getItem(KEY); if (s === 'light' || s === 'dark') return s; } catch (e) {}
        return 'light';
      }
      apply(pref());
      window.bloginToggleTheme = function () {
        var m = root.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
        apply(m);
        try { localStorage.setItem(KEY, m); } catch (e) {}
      };
    })();
    </script>
)JS";
}

std::string_view theme_toggle() {
  return R"HTML(<button type="button" class="blogin-theme-toggle" aria-label="Toggle color theme" title="Toggle color theme" onclick="bloginToggleTheme()"><svg class="icon-moon" viewBox="0 0 24 24" aria-hidden="true"><path d="M21 12.8A8.5 8.5 0 0 1 11.2 3 7 7 0 1 0 21 12.8z"/></svg><svg class="icon-sun" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="4.2"/><line x1="12" y1="2" x2="12" y2="4.5"/><line x1="12" y1="19.5" x2="12" y2="22"/><line x1="2" y1="12" x2="4.5" y2="12"/><line x1="19.5" y1="12" x2="22" y2="12"/><line x1="4.9" y1="4.9" x2="6.7" y2="6.7"/><line x1="17.3" y1="17.3" x2="19.1" y2="19.1"/><line x1="4.9" y1="19.1" x2="6.7" y2="17.3"/><line x1="17.3" y1="6.7" x2="19.1" y2="4.9"/></svg></button>)HTML";
}

}  // namespace blogin::view
