#pragma once

#include <boost/optional.hpp>
#include <future>

namespace sc = std::chrono;

// Helpful wrapper around promise which makes it easy to wait for a
//  timeout and either get nothing or the value
template<typename T>
class SyncValue {
public:
  SyncValue() {
    future_ = promise_.get_future();
  }
  boost::optional<T> WaitForValue(const sc::duration<double>& timeout) {
    auto status = future_.wait_for(timeout);
    if (status == std::future_status::ready) {
      return future_.get();
    } else {
      return boost::none;
    }
  }

  void SetValue(T value) {
    promise_.set_value(value);
  }
protected:
  std::future<T> future_;
  std::promise<T> promise_;
  std::future<T> value_;
};
