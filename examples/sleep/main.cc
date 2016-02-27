#include "ping.hpp"
#include "scheduler.hpp"

#include <chrono>
#include <thread>

using namespace std::literals;

int main(int argc, char* argv[]) {
  Scheduler master_scheduler;
  Ping ping(master_scheduler);

  ping.Start();
  std::this_thread::sleep_for(5s);
  ping.Stop();

  return 0;
}
