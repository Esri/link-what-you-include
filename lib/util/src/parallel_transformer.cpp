// Copyright (c) 2025 Environmental Systems Research Institute, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <util/parallel_transformer.hpp>

#include <cstdlib>
#include <functional>
#include <mutex>
#include <utility>

namespace util
{
Parallel_transformer::Parallel_transformer(size_t thread_count)
{
  threads_.reserve(thread_count);
  for (size_t i = 0; i < thread_count; ++i)
  {
    threads_.emplace_back(
      [this]()
      {
        thread_fun_();
      });
  }
}

Parallel_transformer::~Parallel_transformer()
{
  shutdown_ = true;
  cv_.notify_all();
  for (auto& thread : threads_)
  {
    thread.join();
  }
}

void Parallel_transformer::push_work_(std::function<void()> fun)
{
  std::unique_lock lock(mutex_);
  cv_.wait(lock,
           [this]
           {
             return sync_ == 0 && queue_.size() < 2 * threads_.size();
           });
  queue_.push(std::move(fun));
  ++queued_count_;
  lock.unlock();
  cv_.notify_all();
}

std::function<void()> Parallel_transformer::pop_work_()
{
  std::unique_lock lock(mutex_);
  cv_.wait(lock,
           [this]
           {
             return shutdown_ || !queue_.empty();
           });

  if (shutdown_ && queue_.empty())
  {
    return nullptr;
  }

  auto work = queue_.front();
  queue_.pop();
  lock.unlock();
  cv_.notify_all();

  return work;
}

void Parallel_transformer::flush_()
{
  std::unique_lock lock(mutex_);
  ++sync_;
  cv_.wait(lock,
           [this]
           {
             return compleated_count_ == queued_count_;
           });
  --sync_;
  lock.unlock();
  cv_.notify_all();
}

void Parallel_transformer::thread_fun_()
{
  for (;;)
  {
    auto work = pop_work_();
    if (!work)
    {
      break;
    }

    work();

    {
      std::lock_guard lock(mutex_);
      ++compleated_count_;
    }

    cv_.notify_all();
  }
}

} // namespace util
