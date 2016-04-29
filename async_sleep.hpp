#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "async_task.h"

namespace sc = std::chrono;

// Not thread safe
class AsyncSleep : public AsyncTask {
public:
  AsyncSleep(boost::asio::io_service& io_service) :
    cancelled_(false),
    timer_(io_service) {
  }
  AsyncSleep(AsyncSleep&) = delete;
  AsyncSleep(AsyncSleep&&) = delete;
  AsyncSleep& operator=(AsyncSleep&) = delete;
  AsyncSleep& operator=(AsyncSleep&&) = delete;

  bool Sleep(
      const sc::duration<double>& duration,
      const boost::asio::yield_context& context) {
    if (cancelled_) {
      return false;
    }
    timer_.expires_from_now(boost::posix_time::microseconds(sc::duration_cast<sc::microseconds>(duration).count()));
    boost::system::error_code ec;
    timer_.async_wait(context[ec]);
    return ec == 0;
  }

  virtual void Cancel() override {
    cancelled_ = true;
    timer_.expires_from_now(boost::posix_time::seconds(-1));
  }

//protected:
  bool cancelled_;
  boost::asio::deadline_timer timer_;
};
