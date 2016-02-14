#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>

#include <future>

#include "async_task.h"

// Not thread safe
class AsyncExecutor : public AsyncTask {
public:
  AsyncExecutor(
      boost::asio::io_service& io_service) :
    io_service_(io_service) {}

  // Since the function itself is executed synchrnously, it can
  //  only be cancelled before it has run at all.  Once it has
  //  started, it will finish.
  void Cancel() {
    cancelled_ = true;
  }

  template<typename Functor>
  boost::optional<std::future<typename std::result_of<Functor()>::type>>
  Run(const Functor& func) {
    auto p = std::make_shared<std::promise<typename std::result_of<Functor()>::type>>();
    if (!cancelled_) {
      io_service_.post([p, func]() {
        p->set_value(func());
      });
    } else {
      return boost::none;
    }
    return p->get_future();
  }
      
//protected:
  bool cancelled_ = false;
  //Functor func_;
  boost::asio::io_service& io_service_;
};
