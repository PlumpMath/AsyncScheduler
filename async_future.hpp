#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>

#include "async_semaphore.hpp"
#include "async_task.h"

// Not thread safe
template<typename T>
class AsyncFuture : public AsyncTask {
public:
  AsyncFuture(boost::asio::io_service& io_service) :
    semaphore_(io_service) {
  }

  virtual void Cancel() override {
    cancelled_ = true;
    semaphore_.Cancel();
  }

  boost::optional<T> Get(boost::asio::yield_context& context) {
    if (cancelled_) {
      return boost::none;
    }
    if (value_) {
      return value_;
    }
    if (!semaphore_.Wait(context)) {
      return boost::none;
    }
    return value_;
  }

  void SetValue(T value) {
    if (!cancelled_) {
      value_ = value;
      semaphore_.Raise();
    }
  }

//protected:
  boost::optional<T> value_;
  AsyncSemaphore semaphore_; 
  bool cancelled_ = false;
};
