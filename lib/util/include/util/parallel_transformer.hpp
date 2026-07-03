// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace util
{
class Parallel_transformer
{
public:
  explicit Parallel_transformer(size_t thread_count);
  ~Parallel_transformer();
  Parallel_transformer(const Parallel_transformer&) = delete;
  Parallel_transformer(Parallel_transformer&&) = delete;
  Parallel_transformer& operator=(const Parallel_transformer&) = delete;
  Parallel_transformer& operator=(Parallel_transformer&&) = delete;

  template <class TInputIt, class TOutputIt, class TCallable>
  TOutputIt transform(TInputIt first1, TInputIt last1, TOutputIt d_first, TCallable unary_op)
  {
    for (; first1 != last1; ++d_first, ++first1)
    {
      push_work_(
        [=]()
        {
          *d_first = unary_op(*first1);
        });
    }
    flush_();
    return d_first;
  }

private:
  void push_work_(std::function<void()> fun);
  std::function<void()> pop_work_();
  void flush_();
  void thread_fun_();

  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  uint32_t compleated_count_{0U};
  uint32_t queued_count_{0U};
  uint32_t sync_{0U};
  std::atomic_bool shutdown_{false};
};
} // namespace util
