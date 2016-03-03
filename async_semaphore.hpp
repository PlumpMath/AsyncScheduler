#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "async_task.h"

// Not thread safe, so all logic must execute on the same thread.
//  This is made a bit easier by the fact that all function calls
//  post to the given io_service, so as long as there's only one
//  thread executing it, then things will be fine.  Note: since the
//  Wait method suspends, it must be called on the same thread that's
//  executing the io_service
class AsyncSemaphore : public AsyncTask {
public:
  AsyncSemaphore(boost::asio::io_service& io_service) :
    io_service_(io_service),
    timer_(io_service) {
  }

  // Can be called from any thread, will be posted onto
  //  the execution thread
  virtual void Cancel() override {
    io_service_.dispatch([this]() {
      cancelled_ = true;
      timer_.expires_from_now(boost::posix_time::seconds(-1));
    });
  }

  // This does not post through the io service (since then we woudln't
  //  be able to suspend, at least that i know of), so it must be
  //  called on the same thread that's executing the io service queue.
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

  // Can be called from any thread, will be posted onto
  //  the execution thread
  void Raise() {
    io_service_.dispatch([this]() {
      if (!cancelled_) {
        ++value_;
        timer_.cancel_one();
      }
    });
  }

//protected:
  boost::asio::io_service& io_service_;
  boost::asio::deadline_timer timer_;
  bool cancelled_ = false;
  int value_ = 0;
};
