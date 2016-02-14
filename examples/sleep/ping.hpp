#pragma once

#include <boost/bind.hpp>

#include <chrono>

#include "scheduler.hpp"

using namespace std::literals;

// A class which 'sends' a ping every second
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
      if(!scheduler_.Sleep(1s, context) || !running_) {
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
