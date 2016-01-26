#pragma once

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/asio/spawn.hpp>

#include "async_task.h"

namespace bc = boost::chrono;

class AsyncSleep : public AsyncTask {
public:
  AsyncSleep(boost::asio::io_service& io_service) :
      timer_(io_service) {}
  
  // Returns true if slept for the entire duration, false if it was woken up prematurely
  template <class Rep, class Period>
  bool Sleep(const bc::duration<Rep, Period>& duration, boost::asio::yield_context& context) {
    timer_.expires_from_now(boost::posix_time::microseconds(bc::duration_cast<bc::microseconds>(duration).count()));
    boost::system::error_code ec;
    timer_.async_wait(context[ec]);
    return ec == 0;
  }

  void Cancel() override {
    timer_.expires_from_now(boost::posix_time::seconds(-1));
  }

//protected:
  boost::asio::deadline_timer timer_;
};
