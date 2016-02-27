#pragma once

#include <boost/bind.hpp>

#include "scheduler.hpp"
#include "scheduler_context.hpp"

using namespace std::literals;

bool isHappy(int num) {
  std::set<int> seenNums;

  auto getDigits = [](int num) {
    std::vector<int> digits;
    while (num > 0) {
      int digit = num % 10;
      num /= 10;
      digits.push_back(digit);
    }
    return digits;
  };

  int currNum = num;
  while (currNum != 1 && seenNums.find(currNum) == seenNums.end()) {
    seenNums.insert(currNum);
    auto digits = getDigits(currNum);
    currNum = 0;
    std::for_each(digits.begin(), digits.end(), [&](int n) { currNum += n * n; });
  };

  return currNum == 1;
}

class Coro {
public:
  Coro(Scheduler& scheduler) :
    scheduler_(scheduler) {
  }

  void Start() {
    scheduler_.SpawnCoroutine(boost::bind(&Coro::doStuff, this, _1));
    scheduler_.SpawnCoroutine(boost::bind(&Coro::doOtherStuff, this, _1));
  }

  void doStuff(boost::asio::yield_context context) {
    for (int i = 0; i <= 100; ++i) {
      auto future_handle = scheduler_.Post(std::bind(isHappy, i), UseAsync);
      auto res = scheduler_.WaitOnFuture<bool>(future_handle, context);
      if (res && res.get()) {
        printf("%d is happy\n", i);
      }
      scheduler_.Sleep(100ms, context);
    }
  }

  void doOtherStuff(boost::asio::yield_context context) {
    while (true) {
      if (!scheduler_.Sleep(10ms, context)) {
        break;
      }
      std::cout << "doing stuff\n";
    }
  }

//protected:
  SchedulerContext scheduler_;
};
