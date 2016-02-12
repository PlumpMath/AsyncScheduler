#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

class Scheduler {
public:

  void SpawnCoroutine(std::function<void(boost::asio::yield_context)>&& coro_func) {
    std::promise<bool> coro_completion;

  }


//protected:
  boost::asio::io_service io_service_;
};
