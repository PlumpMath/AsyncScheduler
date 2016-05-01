#include <functional>
#include <chrono>

#include "scheduler_context.hpp"

using namespace std::literals;

// Show two different classes, each with their own SchedulerContext,
//  sharing the same master Scheduler.  Show that shutting down one context
//  doesn't affect the other.

class DoStuffOne {
public:
  DoStuffOne(Scheduler& master_scheduler) :
    scheduler_(master_scheduler) {}

  void Start() {
    scheduler_.SpawnCoroutine(std::bind(&DoStuffOne::DoStuff, this, std::placeholders::_1));
  }

  void Stop() {
    scheduler_.Stop();
  }

  void DoStuff(boost::asio::yield_context context) {
    while (true) {
      if (!scheduler_.Sleep(1s, context)) {
        break;
      }
      std::cout << "doing stuff one\n";
    }
  }

//protected:
  SchedulerContext scheduler_;
};

class DoStuffTwo {
public:
  DoStuffTwo(Scheduler& master_scheduler) :
    scheduler_(master_scheduler) {}

  void Start() {
    scheduler_.SpawnCoroutine(std::bind(&DoStuffTwo::DoStuff, this, std::placeholders::_1));
  }

  void Stop() {
    scheduler_.Stop();
  }

  void DoStuff(boost::asio::yield_context context) {
    while (true) {
      if (!scheduler_.Sleep(1s, context)) {
        break;
      }
      std::cout << "doing stuff two\n";
    }
  }

//protected:
  SchedulerContext scheduler_;
};


int main(int arg, char* argv[]) {
  Scheduler scheduler;

  DoStuffOne one(scheduler);
  DoStuffTwo two(scheduler);

  one.Start();
  two.Start();

  std::this_thread::sleep_for(5s);
  one.Stop();
  std::this_thread::sleep_for(5s);
  two.Stop();

  return 0;
}
