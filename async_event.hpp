#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "async_task.h"

//TODO: at this point, all accesses to this task should all be done
// through a single thread...that way there's no race when notifying
// or cancelling an event
class AsyncEvent : public AsyncTask {
public:
  AsyncEvent(boost::asio::io_service& io_service) :
      timer_(io_service), cancelled_(false), notified_(false) {}

  // Returns true if the event being waited on was fired, false if the
  //  event was cancelled
  bool Wait(const boost::asio::yield_context& context) {
    if (notified_) {
      notified_ = false;
      return true;
    } else if (cancelled_) {
      cancelled_ = false;
      return false;
    }
    boost::system::error_code ec;
    timer_.async_wait(context[ec]);
    if (notified_) {
      notified_ = false;
      return true;
    } else if (cancelled_) {
      cancelled_ = false;
      return false;
    }
    //shouldn't get here
    assert(false);
    return false;
  }

  void Notify() {
    //std::cout << "(AsyncEvent): Notifying event\n";
    notified_ = true;
    timer_.cancel_one();
  }

  void Cancel() override {
    //std::cout << "(AsyncEvent): Cancelling event\n";
    cancelled_ = true;
    timer_.expires_from_now(boost::posix_time::seconds(-1));
  }

//protected:
  boost::asio::deadline_timer timer_;
  std::atomic_bool cancelled_;
  std::atomic_bool notified_;
};
