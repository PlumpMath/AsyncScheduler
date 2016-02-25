#include "coro.hpp"

#include <thread>
#include <chrono>

#include <set>
#include <vector>

using namespace std::literals;


int main(int argc, char* argv[]) {
  Scheduler scheduler;
  Coro coro(scheduler);
  coro.Start();

  std::this_thread::sleep_for(10s);

  return 0;
}
