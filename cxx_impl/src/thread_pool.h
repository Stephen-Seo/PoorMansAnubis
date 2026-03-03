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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_THREAD_POOL_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

class ThreadPool {
 public:
  ThreadPool();

  // disallow copy
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  // allow move
  ThreadPool(ThreadPool &&);
  ThreadPool &operator=(ThreadPool &&);

  void set_thread_count(uint32_t count);

  void add_func(std::function<void(void *)> fn, void *user_data,
                std::function<void(void *)> cleanup_fn);

 private:
  using PendingFnVec =
      std::vector<std::tuple<std::function<void(void *)>, void *,
                             std::function<void(void *)> > >;
  using CondVarTuple = std::tuple<std::mutex, std::condition_variable>;

  static void thread_function(std::shared_ptr<std::atomic_bool> stop_var,
                              std::shared_ptr<CondVarTuple> cond_var,
                              std::shared_ptr<PendingFnVec> pending_fns);

  std::vector<std::thread> thread_handles;
  std::shared_ptr<PendingFnVec> pending_fns;
  std::shared_ptr<std::atomic_bool> stop_var;
  std::shared_ptr<CondVarTuple> cond_var;
};

#endif
