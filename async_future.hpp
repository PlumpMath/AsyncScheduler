#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>

#include "async_semaphore.hpp"
#include "async_task.h"

// Not thread safe, so all logic must execute on the same thread.
//  This is made a bit easier by the fact that all function calls
//  post to the given io_service, so as long as there's only one
//  thread executing it, then things will be fine.  Note: since the
//  Get method suspends, it must be called on the same thread that's
//  executing the io_service
//  TODO: can provide some better guarantees by wrapping the
//  Cancel/SetValue calls in a strand? Is that worth doing if
//  we still can't enforce the calling thread in 'Get'?  if we do
//  a 'dispatch' in Get, does that better convey the requirements?
//  Would that give a better failure mode?
template<typename T>
class AsyncFuture : public AsyncTask {
public:
  AsyncFuture(boost::asio::io_service& io_service) :
    io_service_(io_service),
    semaphore_(io_service) {
  }

  // Can be called from any thread, will be posted onto
  //  the execution thread
  virtual void Cancel() override {
    io_service_.dispatch([this]() {
      cancelled_ = true;
      semaphore_.Cancel();
    });
  }

  // This does not post through the io service (since then we woudln't
  //  be able to suspend, at least that i know of), so it must be
  //  called on the same thread that's executing the io service queue.
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

  // Can be called from any thread, will be posted onto
  //  the execution thread
  void SetValue(T value) {
    io_service_.dispatch([this, v = std::move(value)]() {
      if (!cancelled_) {
        value_ = v;
        semaphore_.Raise();
      }
    });
  }

//protected:
  boost::asio::io_service& io_service_;
  AsyncSemaphore semaphore_; 
  boost::optional<T> value_;
  bool cancelled_ = false;
};
