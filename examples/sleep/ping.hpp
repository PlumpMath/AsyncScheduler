#pragma once

#include <boost/bind.hpp>

#include <chrono>

#include "scheduler.hpp"

using namespace std::literals;

// A class which 'sends' a ping every second
// TODO: have another set of work to do to show the 'sleep'
//  call does not block the thread
class Ping {
public:
  void Start() {
    running_ = true;
    scheduler_.SpawnCoroutine(boost::bind(&Ping::Run, this, _1));
  }

  void Stop() {
    running_ = false;
    scheduler_.Stop();
  }

  void Run(boost::asio::yield_context context) {
    while (running_) {
      SendPing();
      // The decision to exit the coroutine loop is an implementation detail.
      //  Here, the exit logic has been defined as:
      //  "If either the sleep call returned false (meaning it was cancelled before
      //    completing) or 'running_' has been set to false, then we should shut
      //    down and exit"
      if (!scheduler_.Sleep(1s, context) || !running_) {
        std::cout << "Stopping\n";
        break;
      }
    }
  }

  void SendPing() {
    std::cout << "Ping\n";
  }

//protected:
  bool running_ = false;
  Scheduler scheduler_;
};
