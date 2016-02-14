#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <future>

#include "async_task.h"

// Not thread safe
class AsyncExecutor : public AsyncTask {
public:
  AsyncExecutor(
      boost::asio::io_service& io_service,
      const std::function<void()>& func) :
    func_(func),
    io_service_(io_service) {}

  // Since the function itself is executed synchrnously, it can
  //  only be cancelled before it has run at all.  Once it has
  //  started, it will finish.
  void Cancel() {
    cancelled_ = true;
  }

  //TODO: have this return a std::future<boost::optional<T>> where T
  // is the return value of the func. (allow for more flexibility on the
  // given function and get its return value)
  std::future<bool> Run() {
    auto p = std::make_shared<std::promise<bool>>();
    if (!cancelled_) {
      io_service_.post([p, this]() {
        func_();
        p->set_value(true);
      });
    } else {
      p->set_value(false);
    }
    return p->get_future();
  }
      
//protected:
  bool cancelled_ = false;
  std::function<void()> func_;
  boost::asio::io_service& io_service_;
};
