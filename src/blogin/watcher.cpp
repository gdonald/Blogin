#include "watcher.h"

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <format>
#include <mutex>

#include <fcntl.h>
#include <unistd.h>

#include "files.h"

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#elifdef __linux__
#include <poll.h>
#include <sys/inotify.h>
#endif

namespace blogin {
namespace {

#ifdef __linux__

ParseError limit_error(std::size_t wanted, std::string_view knob) {
  return ParseError{
    std::format("cannot watch {} directories: the per-user limit was reached. Raise {} and try again",
                wanted, knob),
    1, 1};
}

#endif

#ifdef __APPLE__

// FSEvents watches whole trees from one stream, reports writes and not only
// directory-entry changes, and costs no descriptor per directory. It delivers on
// a dispatch queue, so there is no run loop to keep turning.
class FsEventsWatcher final : public Watcher {
public:
  static std::expected<std::unique_ptr<Watcher>, ParseError> open(
    const std::vector<std::filesystem::path>& roots,
    const std::vector<std::filesystem::path>& excluded) {
    auto watcher = std::unique_ptr<FsEventsWatcher>(new FsEventsWatcher());
    watcher->watched_ = roots.size();

    std::error_code error;

    for (const std::filesystem::path& path : excluded) {
      watcher->excluded_.push_back(std::filesystem::weakly_canonical(path, error).generic_string());
    }

    // The stream is recursive, so the shortest paths already cover the rest.
    std::vector<std::string> tops;

    for (const std::filesystem::path& path : roots) {
      const std::string resolved = std::filesystem::weakly_canonical(path, error).generic_string();
      bool covered = false;

      for (std::string& already : tops) {
        if (resolved.starts_with(already + "/")) {
          covered = true;

          break;
        }

        if (already.starts_with(resolved + "/")) {
          already = resolved;
          covered = true;

          break;
        }
      }

      if (!covered) {
        tops.push_back(resolved);
      }
    }

    std::sort(tops.begin(), tops.end());
    tops.erase(std::unique(tops.begin(), tops.end()), tops.end());

    watcher->tops_ = tops;

    CFMutableArrayRef paths = CFArrayCreateMutable(nullptr, 0, &kCFTypeArrayCallBacks);

    for (const std::string& top : tops) {
      CFStringRef entry = CFStringCreateWithCString(nullptr, top.c_str(), kCFStringEncodingUTF8);

      CFArrayAppendValue(paths, entry);
      CFRelease(entry);
    }

    FSEventStreamContext context{};
    context.info = watcher.get();

    watcher->stream_ = FSEventStreamCreate(
      nullptr, &FsEventsWatcher::deliver, &context, paths, kFSEventStreamEventIdSinceNow, 0.0,
      kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);

    CFRelease(paths);

    if (watcher->stream_ == nullptr) {
      return std::unexpected(ParseError{"cannot watch for file changes", 1, 1});
    }

    watcher->queue_ = dispatch_queue_create("dev.blogin.watcher", nullptr);

    FSEventStreamSetDispatchQueue(watcher->stream_, watcher->queue_);

    if (FSEventStreamStart(watcher->stream_) == 0) {
      return std::unexpected(ParseError{"cannot start watching for file changes", 1, 1});
    }

    return watcher;
  }

  ~FsEventsWatcher() override {
    if (stream_ != nullptr) {
      FSEventStreamStop(stream_);
      FSEventStreamInvalidate(stream_);
      FSEventStreamRelease(stream_);
    }

    if (queue_ != nullptr) {
      // Invalidating unschedules the stream but says nothing about a callback
      // already running on the queue. The queue is serial, so a no-op put on it
      // and waited for cannot start until that callback has returned. Without
      // this, the members below are destroyed while deliver is still reading
      // them, which ThreadSanitizer reports as a race and which is a
      // use-after-free whether or not it is reported.
      dispatch_sync_f(queue_, nullptr, [](void*) {});

      dispatch_release(queue_);
    }
  }

  bool wait(std::chrono::milliseconds timeout) override {
    std::unique_lock guard(mutex_);

    ready_.wait_for(guard, timeout, [this] { return changed_ || stopping_; });

    if (!changed_) {
      return false;
    }

    changed_ = false;

    return true;
  }

  void stop() override {
    {
      const std::scoped_lock guard(mutex_);

      stopping_ = true;
    }

    ready_.notify_all();
  }

  std::size_t watched() const override { return watched_; }

private:
  FsEventsWatcher() = default;

  static void deliver(ConstFSEventStreamRef /*stream*/, void* info, std::size_t count, void* paths,
                      const FSEventStreamEventFlags* /*flags*/, const FSEventStreamEventId* /*ids*/) {
    auto* watcher = static_cast<FsEventsWatcher*>(info);
    auto** changed = static_cast<char**>(paths);

    bool interesting = false;

    for (std::size_t index = 0; index < count; ++index) {
      if (!watcher->ignored(changed[index])) {
        interesting = true;

        break;
      }
    }

    if (!interesting) {
      return;
    }

    {
      const std::scoped_lock guard(watcher->mutex_);

      watcher->changed_ = true;
    }

    watcher->ready_.notify_all();
  }

  bool ignored(std::string_view path) const {
    return ignored_change(path, tops_, excluded_);
  }

  FSEventStreamRef stream_ = nullptr;
  dispatch_queue_t queue_ = nullptr;
  std::vector<std::string> excluded_;
  std::vector<std::string> tops_;
  std::size_t watched_ = 0;

  mutable std::mutex mutex_;
  std::condition_variable ready_;
  bool changed_ = false;
  bool stopping_ = false;
};

#elifdef __linux__

// A pipe, so a blocked poll can be woken from another thread.
class Interrupt {
public:
  Interrupt() {
    // Non-blocking on purpose. Draining reads until the pipe is empty, and on a
    // blocking pipe that last read never returns.
    if (::pipe2(fds_, O_NONBLOCK) != 0) {
      fds_[0] = -1;
      fds_[1] = -1;
    }
  }

  ~Interrupt() {
    for (const int descriptor : fds_) {
      if (descriptor >= 0) {
        ::close(descriptor);
      }
    }
  }

  Interrupt(const Interrupt&) = delete;
  Interrupt& operator=(const Interrupt&) = delete;

  void raise() const {
    if (fds_[1] >= 0) {
      const char byte = 'x';

      [[maybe_unused]] const ssize_t written = ::write(fds_[1], &byte, 1);
    }
  }

  void drain() const {
    char buffer[64];

    while (::read(fds_[0], buffer, sizeof(buffer)) > 0) {
    }
  }

  int reader() const { return fds_[0]; }

private:
  int fds_[2] = {-1, -1};
};

class InotifyWatcher final : public Watcher {
public:
  static std::expected<std::unique_ptr<Watcher>, ParseError> open(
    const std::vector<std::filesystem::path>& roots) {
    auto watcher = std::unique_ptr<InotifyWatcher>(new InotifyWatcher());

    watcher->handle_ = ::inotify_init1(IN_NONBLOCK);

    if (watcher->handle_ < 0) {
      return std::unexpected(ParseError{"cannot create an inotify watch", 1, 1});
    }

    for (const std::filesystem::path& directory : roots) {
      const int watch = ::inotify_add_watch(
        watcher->handle_, directory.c_str(),
        IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE);

      if (watch < 0) {
        // The cap is per user and easy to swallow, and swallowing it leaves a
        // preview that has silently stopped noticing edits.
        if (errno == ENOSPC) {
          return std::unexpected(limit_error(roots.size(), "fs.inotify.max_user_watches"));
        }

        continue;
      }

      ++watcher->watched_;
    }

    return watcher;
  }

  ~InotifyWatcher() override {
    if (handle_ >= 0) {
      ::close(handle_);
    }
  }

  bool wait(std::chrono::milliseconds timeout) override {
    struct pollfd waiting[2] {};

    waiting[0].fd = handle_;
    waiting[0].events = POLLIN;
    waiting[1].fd = interrupt_.reader();
    waiting[1].events = POLLIN;

    const int ready = ::poll(waiting, 2, static_cast<int>(timeout.count()));

    if (ready <= 0) {
      return false;
    }

    if ((waiting[1].revents & POLLIN) != 0) {
      interrupt_.drain();
    }

    if ((waiting[0].revents & POLLIN) == 0) {
      return false;
    }

    // The events themselves are drained and discarded: what changed is the
    // build's business, and inotify's answer would be incomplete anyway.
    char buffer[4096];
    bool changed = false;

    while (::read(handle_, buffer, sizeof(buffer)) > 0) {
      changed = true;
    }

    return changed;
  }

  void stop() override { interrupt_.raise(); }

  std::size_t watched() const override { return watched_; }

private:
  InotifyWatcher() = default;

  int handle_ = -1;
  std::size_t watched_ = 0;
  Interrupt interrupt_;
};

#endif

}  // namespace

bool ignored_change(std::string_view path, const std::vector<std::string>& roots,
                    const std::vector<std::string>& excluded) {
  for (const std::string& prefix : excluded) {
    if (path == prefix || path.starts_with(prefix + "/")) {
      return true;
    }
  }

  for (const std::string& root : roots) {
    if (!path.starts_with(root + "/")) {
      continue;
    }

    std::string_view rest = path.substr(root.size() + 1);

    // Only the directory components count. The last one is the file itself, and
    // a dotfile written on purpose, such as .keep or static/.htaccess, is an
    // edit like any other.
    for (std::size_t slash = rest.find('/'); slash != std::string_view::npos;
         slash = rest.find('/')) {
      if (rest.front() == '.') {
        return true;
      }

      rest = rest.substr(slash + 1);
    }

    return false;
  }

  return false;
}

std::vector<std::filesystem::path> watchable_directories(const std::filesystem::path& root,
                                                         const std::filesystem::path& output) {
  std::vector<std::filesystem::path> directories;
  std::error_code error;

  const std::filesystem::path resolved_output = std::filesystem::weakly_canonical(output, error);

  for (const std::filesystem::path& directory : files::descendant_directories(root)) {
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(directory, error);

    // Watching the output would mean a build waking the watcher that started it.
    if (!error && (resolved == resolved_output || files::within(resolved, resolved_output))) {
      continue;
    }

    directories.push_back(directory);
  }

  return directories;
}

std::expected<std::unique_ptr<Watcher>, ParseError> Watcher::watch(
  const std::vector<std::filesystem::path>& roots,
  const std::vector<std::filesystem::path>& excluded) {
  // Checked here so both backends answer the same way. A watcher over no
  // directories reports every wait as a timeout, which reads as a preview that
  // has stopped noticing edits.
  if (roots.empty()) {
    return std::unexpected(ParseError{"nothing to watch", 1, 1});
  }

#ifdef __APPLE__
  return FsEventsWatcher::open(roots, excluded);
#elifdef __linux__
  // inotify watches each directory on its own, so it never sees the output tree
  // in the first place.
  (void)excluded;

  return InotifyWatcher::open(roots);
#else
  (void)roots;
  (void)excluded;

  return std::unexpected(ParseError{"watching files is not supported on this platform", 1, 1});
#endif
}

}  // namespace blogin
