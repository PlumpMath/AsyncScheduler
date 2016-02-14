#include "ping.hpp"

#include <chrono>
#include <thread>

using namespace std::literals;

int main(int argc, char* argv[]) {
  Ping ping;

  ping.Start();
  std::this_thread::sleep_for(5s);
  ping.Stop();

  return 0;
}
