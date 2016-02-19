#pragma once

#include <boost/optional.hpp>

#include <list>

#include "scheduler.hpp"
#include "async_semaphore.hpp"


// AsyncBuffer allows a caller to asynchronously wait until there
//  is data to be read
template<typename T>
class AsyncBuffer {
public:
  AsyncBuffer(Scheduler& scheduler) :
    scheduler_(scheduler),
    have_data_semaphore_(scheduler_.CreateSemaphore()) {
  }

  void Write(T&& value) {
    {
      std::lock_guard<std::mutex> guard(buffer_mutex_);
      buffer_.push_back(std::move(value));
    }
    scheduler_.RaiseSemaphore(have_data_semaphore_);
  }

  boost::optional<T> Read(boost::asio::yield_context& context) {
    if (scheduler_.WaitOnSemaphore(have_data_semaphore_, context)) {
      std::lock_guard<std::mutex> guard(buffer_mutex_);
      T value = std::move(buffer_.front());
      buffer_.pop_front();
      return value;
    }
    return boost::none;
  }

//protected:
  std::mutex buffer_mutex_;
  std::list<T> buffer_;
  Scheduler& scheduler_;
  Scheduler::task_handle_t have_data_semaphore_;
};
