#pragma once

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/optional.hpp>

// Helpful wrapper around condition variable which encapsulates all
//  the locking, setting, and waiting logic
template<typename T>
class SyncValue {
public:
  SyncValue() : condition_(false) {}
  boost::optional<T> WaitForValue(const boost::chrono::milliseconds& timeout) {
    boost::mutex::scoped_lock lock(mutex_);
    // If we don't already have the value, wait for it
    if (!value_) {
      cond_.wait_for(lock, timeout);
    }
    // By here, we either already had the value or we timed out.
    //  Either way, return whatever we've got (which will either be
    //  the actual value or an empty optional)
    return value_;
  }
  void SetValue(T value) {
    boost::mutex::scoped_lock lock(mutex_);
    value_ = value;
    cond_.notify_one();
  }
protected:
  boost::condition_variable cond_;
  boost::optional<T> value_;
  bool condition_;
  boost::mutex mutex_;
};
