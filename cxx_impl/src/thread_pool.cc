// ISC License
//
// Copyright (c) 2025-2026 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include "thread_pool.h"

#include <optional>

void ThreadPool::thread_function(
    std::shared_ptr<std::atomic_bool> stop,
    std::shared_ptr<CondVarTuple> cond_var,
    std::shared_ptr<ThreadPool::PendingFnVec> pending_fns) {
  std::optional<std::function<void(void *)> > fn = std::nullopt;
  std::optional<void *> ud = std::nullopt;
  std::optional<std::function<void(void *)> > cleanup_fn = std::nullopt;
  while (!stop->load(std::memory_order_acquire)) {
    {
      std::unique_lock<std::mutex> lock(std::get<std::mutex>(*cond_var));
      if (pending_fns->empty()) {
        std::get<std::condition_variable>(*cond_var).wait(lock);
      }

      if (!pending_fns->empty()) {
        fn = std::move(std::get<0>(pending_fns->back()));
        ud = std::get<1>(pending_fns->back());
        cleanup_fn = std::move(std::get<2>(pending_fns->back()));
        pending_fns->pop_back();
      }
    }

    if (fn.has_value() && ud.has_value() && cleanup_fn.has_value()) {
      fn.value()(ud.value());
      cleanup_fn.value()(ud.value());

      fn = std::nullopt;
      ud = std::nullopt;
      cleanup_fn = std::nullopt;
    }
  }
}

ThreadPool::ThreadPool()
    : pending_fns(std::make_shared<PendingFnVec>()),
      stop_var(std::make_shared<std::atomic_bool>(false)),
      cond_var(std::make_shared<CondVarTuple>()) {}

ThreadPool::~ThreadPool() {
  stop_var->store(true, std::memory_order_release);
  std::get<std::condition_variable>(*cond_var).notify_all();

  for (auto iter = thread_handles.begin(); iter != thread_handles.end();
       ++iter) {
    iter->join();
  }

  for (auto iter = pending_fns->begin(); iter != pending_fns->end(); ++iter) {
    // Call cleanup_fn on userdata.
    std::get<2> (*iter)(std::get<1>(*iter));
  }
}

ThreadPool::ThreadPool(ThreadPool &&other)
    : thread_handles(std::move(other.thread_handles)),
      pending_fns(std::move(other.pending_fns)),
      stop_var(std::move(other.stop_var)),
      cond_var(std::move(other.cond_var)) {}

ThreadPool &ThreadPool::operator=(ThreadPool &&other) {
  stop_var->store(true, std::memory_order_release);
  std::get<std::condition_variable>(*cond_var).notify_all();

  for (auto iter = thread_handles.begin(); iter != thread_handles.end();
       ++iter) {
    iter->join();
  }

  thread_handles.clear();

  thread_handles = std::move(other.thread_handles);
  {
    std::unique_lock<std::mutex> lock(std::get<0>(*other.cond_var));
    // thread_handles in "other" has a reference to "other.pending_fns".
    // Populate "other.pending_fns" with still-not-executed fns.
    while (!pending_fns->empty()) {
      other.pending_fns->push_back(std::move(pending_fns->back()));
      pending_fns->pop_back();
    }
  }
  pending_fns = std::move(other.pending_fns);
  stop_var = std::move(other.stop_var);
  cond_var = std::move(other.cond_var);

  return *this;
}

void ThreadPool::set_thread_count(uint32_t count) {
  stop_var->store(true, std::memory_order_release);
  std::get<std::condition_variable>(*cond_var).notify_all();

  for (auto iter = thread_handles.begin(); iter != thread_handles.end();
       ++iter) {
    iter->join();
  }

  thread_handles.clear();

  stop_var = std::make_shared<std::atomic_bool>(false);
  cond_var = std::make_shared<CondVarTuple>();

  for (uint32_t idx = 0; idx < count; ++idx) {
    thread_handles.emplace_back(thread_function, stop_var, cond_var,
                                pending_fns);
  }
}

void ThreadPool::add_func(std::function<void(void *)> fn, void *user_data,
                          std::function<void(void *)> cleanup_fn) {
  {
    std::unique_lock<std::mutex> lock(std::get<std::mutex>(*cond_var));
    pending_fns->push_back(
        std::make_tuple(std::move(fn), user_data, std::move(cleanup_fn)));
  }
  std::get<std::condition_variable>(*cond_var).notify_one();

  if (thread_handles.empty()) {
    // Handle the case of "set_thread_count(...)" not being called before this.
    set_thread_count(1);
  }
}
