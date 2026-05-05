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

#include "thread_limit.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <thread>

#include "poor_mans_print.h"

void internal_thread_function(std::function<void(void *)> fn, void *user_data,
                              std::function<void(void *)> cleanup_fn,
                              std::shared_ptr<std::atomic_uint64_t> counter,
                              std::shared_ptr<std::condition_variable> cv) {
  try {
    fn(user_data);
  } catch (const std::exception &e) {
    PMA_EPrintln("WARNING: Thread threw exception during execution: {}",
                 e.what());
  }
  try {
    cleanup_fn(user_data);
  } catch (const std::exception &e) {
    PMA_EPrintln("WARNING: Thread threw exception during cleanup: {}",
                 e.what());
  }
  --(*counter);
  cv->notify_all();
}

void internal_manager_function(
    uint64_t limit, std::shared_ptr<std::atomic_uint64_t> counter,
    std::shared_ptr<std::condition_variable> cv,
    std::shared_ptr<std::atomic_bool> stop_flag,
    std::shared_ptr<std::mutex> data_mutex,
    std::shared_ptr<std::list<ThreadLimit::ThreadData> > overflow_data) {
  std::unique_lock<std::mutex> lock(*data_mutex);
  while (!stop_flag->load()) {
    while (counter->load() < limit && !overflow_data->empty()) {
      ThreadLimit::ThreadData data = std::move(overflow_data->front());
      overflow_data->pop_front();

      ++(*counter);
      std::thread new_thread(internal_thread_function,
                             std::move(std::get<0>(data)), std::get<1>(data),
                             std::move(std::get<2>(data)), counter, cv);
      new_thread.detach();
    }

    cv->wait(lock);
  }
}

ThreadLimit::ThreadLimit(uint64_t limit)
    : limit(limit),
      counter(std::make_shared<std::atomic_uint64_t>(0)),
      cv(std::make_shared<std::condition_variable>()),
      stop_flag(std::make_shared<std::atomic_bool>(false)),
      data_mutex(std::make_shared<std::mutex>()),
      overflow_data(std::make_shared<std::list<ThreadLimit::ThreadData> >()),
      internal_manager_thread() {
  internal_manager_thread =
      std::make_shared<std::thread>(internal_manager_function, limit, counter,
                                    cv, stop_flag, data_mutex, overflow_data);
}

ThreadLimit::~ThreadLimit() {
  if (!stop_flag || !cv || !internal_manager_thread) {
    return;
  }

  stop_flag->store(true);
  cv->notify_all();
  internal_manager_thread->join();

  if (overflow_data) {
    for (auto iter = overflow_data->begin(); iter != overflow_data->end();
         ++iter) {
      try {
        std::get<2> (*iter)(std::get<1>(*iter));
      } catch (const std::exception &e) {
        PMA_EPrintln(
            "WARNING: cleanup during ~ThreadLimit() threw exception: {}",
            e.what());
      }
    }
  }

  if (counter) {
    while (counter->load() > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

ThreadLimit::ThreadLimit(ThreadLimit &&other)
    : limit(other.limit),
      counter(other.counter),
      cv(other.cv),
      stop_flag(other.stop_flag),
      data_mutex(other.data_mutex),
      overflow_data(other.overflow_data),
      internal_manager_thread(other.internal_manager_thread) {
  other.counter.reset();
  other.cv.reset();
  other.stop_flag.reset();
  other.data_mutex.reset();
  other.overflow_data.reset();
  other.internal_manager_thread.reset();
}

ThreadLimit &ThreadLimit::operator=(ThreadLimit &&other) {
  if (stop_flag && cv && internal_manager_thread && counter) {
    stop_flag->store(true);
    cv->notify_all();
    internal_manager_thread->join();

    while (counter->load() > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  limit = other.limit;
  counter = other.counter;
  cv = other.cv;
  stop_flag = other.stop_flag;
  data_mutex = other.data_mutex;
  overflow_data = other.overflow_data;
  internal_manager_thread = other.internal_manager_thread;

  other.counter.reset();
  other.cv.reset();
  other.stop_flag.reset();
  other.data_mutex.reset();
  other.overflow_data.reset();
  other.internal_manager_thread.reset();

  return *this;
}

void ThreadLimit::add_thread(std::function<void(void *)> fn, void *user_data,
                             std::function<void(void *)> cleanup_fn) {
  if (counter->load(std::memory_order_acquire) >= limit) {
    std::lock_guard<std::mutex> lock(*this->data_mutex);
    overflow_data->push_back(
        std::make_tuple(std::move(fn), user_data, std::move(cleanup_fn)));
  } else {
    ++(*counter);
    std::thread new_thread(internal_thread_function, std::move(fn), user_data,
                           std::move(cleanup_fn), counter, cv);
    new_thread.detach();
  }
}
