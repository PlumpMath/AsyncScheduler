#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "async_task.h"

// Not thread safe
class AsyncSemaphore : public AsyncTask {
public:
  AsyncSemaphore(boost::asio::io_service& io_service) :
    timer_(io_service) {
  }

  virtual void Cancel() override {
    cancelled_ = true;
    timer_.expires_from_now(boost::posix_time::seconds(-1));
  }

  bool Wait(boost::asio::yield_context& context) {
    if (cancelled_) {
      return false;
    }
    if (value_ > 0) {
      --value_;
      return true;
    } else {
      boost::system::error_code ec;
      timer_.async_wait(context[ec]);
      if (cancelled_) {
        return false;
      }
      assert(value_ > 0);
      --value_;
      return true;
    }
  }

  void Raise() {
    ++value_;
    timer_.cancel_one();
  }

//protected:
  bool cancelled_ = false;
  int value_ = 0;
  boost::asio::deadline_timer timer_;
};
