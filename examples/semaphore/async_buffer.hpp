#pragma once

#include <boost/optional.hpp>

#include <list>

#include "scheduler.hpp"
#include "async_semaphore.hpp"


// AsyncBuffer allows a caller to asynchronously wait until there
//  is data to be read
// Not thread safe.
template<typename T>
class AsyncBuffer {
public:
  AsyncBuffer(Scheduler& scheduler) :
    scheduler_(scheduler),
    have_data_semaphore_(scheduler_.CreateSemaphore().get()) {
  }

  void Write(T&& value) {
    buffer_.push_back(std::move(value));
    scheduler_.RaiseSemaphore(have_data_semaphore_);
  }

  boost::optional<T> Read(boost::asio::yield_context& context) {
    if (scheduler_.WaitOnSemaphore(have_data_semaphore_, context)) {
      T value = buffer_.front();
      buffer_.pop_front();
      return value;
    }
    return boost::none;
  }

//protected:
  std::list<T> buffer_;
  Scheduler& scheduler_;
  int have_data_semaphore_;
};
