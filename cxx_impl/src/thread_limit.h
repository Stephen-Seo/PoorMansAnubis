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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_THREAD_LIMIT_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_THREAD_LIMIT_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <mutex>

class ThreadLimit {
 public:
  using ThreadData = std::tuple<std::function<void(void *)>, void *,
                                std::function<void(void *)> >;

  ThreadLimit(uint64_t limit);
  ~ThreadLimit();

  // disallow copy
  ThreadLimit(const ThreadLimit &) = delete;
  ThreadLimit &operator=(const ThreadLimit &) = delete;

  // allow move
  ThreadLimit(ThreadLimit &&);
  ThreadLimit &operator=(ThreadLimit &&);

  void add_thread(std::function<void(void *)> fn, void *user_data,
                  std::function<void(void *)> cleanup_fn);

 private:
  uint64_t limit;
  std::shared_ptr<std::atomic_uint64_t> counter;
  std::shared_ptr<std::condition_variable> cv;
  std::shared_ptr<std::atomic_bool> stop_flag;
  std::shared_ptr<std::mutex> data_mutex;
  std::shared_ptr<std::list<ThreadData> > overflow_data;
  std::shared_ptr<std::thread> internal_manager_thread;
};

#endif
