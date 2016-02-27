#pragma once

#include <boost/bind.hpp>

#include <chrono>

#include "scheduler_context.hpp"

using namespace std::literals;

// A class which 'sends' a ping every second and does some other
//  work inbetween.
class Ping {
public:
  Ping(Scheduler& master_scheduler) : scheduler_(master_scheduler) {}
  void Start() {
    running_ = true;
    scheduler_.SpawnCoroutine(boost::bind(&Ping::Run, this, _1));
    scheduler_.SpawnCoroutine(boost::bind(&Ping::DoOtherStuff, this, _1));
  }

  void Stop() {
    running_ = false;
    scheduler_.Stop();
  }

  void DoOtherStuff(boost::asio::yield_context context) {
    while (running_) {
      printf("Doing other stuff\n");
      if (!scheduler_.Sleep(2s, context) || !running_) {
        std::cout << "Stopping other stuff\n";
        break;
      }
    }
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
  SchedulerContext scheduler_;
};
