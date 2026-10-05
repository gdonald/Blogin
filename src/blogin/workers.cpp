#include "workers.h"

#include <pthread.h>

#include <vector>

namespace blogin {
namespace {

void* run_task(void* task) {
  (*static_cast<const std::function<void()>*>(task))();

  return nullptr;
}

}  // namespace

void run_on_workers(unsigned count, const std::function<void()>& task) {
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, worker_stack_bytes);

  std::vector<pthread_t> threads;
  threads.reserve(count);

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) pthread takes the argument as void*
  void* argument = const_cast<std::function<void()>*>(&task);

  for (unsigned index = 0; index < count; ++index) {
    pthread_t thread{};

    if (pthread_create(&thread, &attributes, run_task, argument) == 0) {
      threads.push_back(thread);
    }
  }

  pthread_attr_destroy(&attributes);

  // A thread the system refused to start leaves its share of the work to the
  // others, since every task pulls from a shared queue. With none started,
  // whether refused or not asked for, the calling thread does it.
  if (threads.empty()) {
    task();
  }

  for (const pthread_t thread : threads) {
    pthread_join(thread, nullptr);
  }
}

}  // namespace blogin
