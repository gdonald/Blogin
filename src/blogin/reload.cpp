#include "reload.h"

#include <chrono>
#include <format>
#include <unistd.h>

#include "json.h"
#include "value.h"

namespace blogin::reload {
namespace {

// Carried over unchanged. It decides what a change means for the page it is on:
// its own file means a reload, a stylesheet means a swapped href, an image means
// a swapped src, and a script means a reload because a loaded script cannot be
// taken back.
constexpr std::string_view client = R"JS(
var known = blogin.version;
var retry = 500;

function normalize(path) {
  var clean = path.split("?")[0].split("#")[0];

  if (clean.charAt(0) === "/") clean = clean.slice(1);
  if (clean === "" || clean.charAt(clean.length - 1) === "/") return clean + "index.html";

  var last = clean.split("/").pop();

  return last.indexOf(".") < 0 ? clean + ".html" : clean;
}

function bust(url) {
  return url.split("?")[0] + "?blogin=" + known;
}

function nodesFor(selector, attribute, changed) {
  var nodes = document.querySelectorAll(selector);
  var found = [];

  for (var i = 0; i < nodes.length; i++) {
    var value = nodes[i].getAttribute(attribute);

    if (!value) continue;

    var current = normalize(new URL(value, location.href).pathname);

    if (changed.indexOf(current) >= 0) found.push(nodes[i]);
  }

  return found;
}

function apply(paths) {
  var here = normalize(location.pathname);
  var styles = [];
  var scripts = [];
  var images = [];

  for (var i = 0; i < paths.length; i++) {
    var path = normalize(paths[i]);

    if (path === here) {
      location.reload();
      return;
    }

    if (/\.css$/.test(path)) styles.push(path);
    else if (/\.js$/.test(path)) scripts.push(path);
    else if (/\.(png|jpe?g|gif|svg|webp|avif|ico)$/.test(path)) images.push(path);
  }

  if (scripts.length && nodesFor("script[src]", "src", scripts).length) {
    location.reload();
    return;
  }

  nodesFor('link[rel="stylesheet"]', "href", styles).forEach(function (link) {
    link.setAttribute("href", bust(link.getAttribute("href")));
  });

  nodesFor("img", "src", images).forEach(function (image) {
    image.setAttribute("src", bust(image.getAttribute("src")));
  });
}

function overlay(message) {
  var box = document.getElementById("blogin-failure");

  if (!message) {
    if (box) box.parentNode.removeChild(box);
    return;
  }

  if (!box) {
    box = document.createElement("div");
    box.id = "blogin-failure";
    box.setAttribute("style", "position:fixed;inset:0;z-index:2147483647;background:rgba(12,14,18,0.94);color:#f2f4f8;font:14px/1.6 ui-monospace,SFMono-Regular,Menlo,monospace;padding:2rem;overflow:auto;white-space:pre-wrap");
    document.body.appendChild(box);
  }

  box.textContent = message;
}

function describe(message) {
  var out = "Build failed\n\n" + message.detail;

  if (message.file) out += "\n\n" + message.file + ":" + message.line;

  return out;
}

function connect() {
  var scheme = location.protocol === "https:" ? "wss://" : "ws://";
  var socket = new WebSocket(scheme + location.host + blogin.path);

  socket.onopen = function () { retry = 500; };

  socket.onmessage = function (event) {
    var message = JSON.parse(event.data);

    if (message.type === "hello") {
      if (message.session !== blogin.session || message.version !== known) location.reload();
      return;
    }

    if (message.type === "failure") {
      known = message.version;
      overlay(describe(message));
      return;
    }

    known = message.version;
    overlay(null);
    apply(message.paths || []);
  };

  socket.onclose = function () {
    setTimeout(connect, retry);
    retry = Math.min(retry * 2, 5000);
  };
}

connect();
)JS";

}  // namespace

std::string hello_message(std::int64_t version, std::string_view session) {
  Value message = Value::object();
  message.set("type", Value("hello"));
  message.set("version", Value(version));
  message.set("session", Value(std::string(session)));

  return to_json(message, JsonStyle::compact);
}

std::string change_message(std::int64_t version, const std::vector<std::string>& paths) {
  Value listed = Value::array();

  for (const std::string& path : paths) {
    listed.push(Value(path));
  }

  Value message = Value::object();
  message.set("type", Value("change"));
  message.set("version", Value(version));
  message.set("paths", std::move(listed));

  return to_json(message, JsonStyle::compact);
}

std::string failure_message(std::int64_t version, std::string_view detail, std::string_view file,
                            std::size_t line) {
  Value message = Value::object();
  message.set("type", Value("failure"));
  message.set("version", Value(version));
  message.set("detail", Value(std::string(detail)));
  message.set("file", Value(std::string(file)));
  message.set("line", Value(static_cast<std::int64_t>(line)));

  return to_json(message, JsonStyle::compact);
}

std::vector<std::string> changed_paths(const std::vector<std::filesystem::path>& files,
                                       const std::filesystem::path& root) {
  std::vector<std::string> paths;

  for (const std::filesystem::path& file : files) {
    std::string relative = file.lexically_relative(root).generic_string();

    if (relative.empty() || relative.starts_with("..")) {
      relative = file.generic_string();
    }

    if (std::find(paths.begin(), paths.end(), relative) == paths.end()) {
      paths.push_back(std::move(relative));
    }
  }

  return paths;
}

std::string client_script(std::int64_t version, std::string_view session) {
  Value settings = Value::object();
  settings.set("path", Value(std::string(socket_path)));
  settings.set("version", Value(version));
  settings.set("session", Value(std::string(session)));

  return std::format("<script>(function () {{\nvar blogin = {};\n{}}})();</script>",
                     to_json(settings, JsonStyle::compact), client);
}

std::string inject(std::string_view html, std::string_view script) {
  const auto closing = html.find("</body>");

  if (closing == std::string_view::npos) {
    return std::string(html) + std::string(script);
  }

  std::string out(html.substr(0, closing));
  out += script;
  out += html.substr(closing);

  return out;
}

Config preview_config(Config config) {
  config.minify = false;
  config.fingerprint = false;
  config.output_dir = preview_output_dir;

  return config;
}

Channel::Channel() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();

  // Enough to tell one run of the server from the next, which is all a page
  // needs in order to notice it is talking to a different server than the one
  // that wrote it.
  session_ = std::format("{}-{}", static_cast<long long>(::getpid()),
                         std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

std::string Channel::change(const std::vector<std::string>& paths) {
  const std::scoped_lock guard(mutex_);

  ++version_;

  return change_message(version_, paths);
}

std::string Channel::failure(std::string_view detail, std::string_view file, std::size_t line) {
  const std::scoped_lock guard(mutex_);

  ++version_;

  return failure_message(version_, detail, file, line);
}

std::string Channel::hello() const {
  const std::scoped_lock guard(mutex_);

  return hello_message(version_, session_);
}

std::int64_t Channel::version() const {
  const std::scoped_lock guard(mutex_);

  return version_;
}

}  // namespace blogin::reload
