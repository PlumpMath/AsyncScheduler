#include <chrono>

#include "async_buffer.hpp"

#include "scheduler.hpp"
#include "scheduler_context.hpp"

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  Scheduler master_scheduler;
  AsyncBuffer<int> buffer(master_scheduler);
  SchedulerContext scheduler(master_scheduler);
  bool running = true;

  boost::thread writer([&]() {
    int i = 0;
    while (running) {
      std::this_thread::sleep_for(300ms);
      printf("writing val %d\n", i);
      buffer.Write(i++);
    }
  });

  auto reader = [&](boost::asio::yield_context context) {
    while (running) {
      auto val = buffer.Read(context);
      if (val) {
        printf("got val %d\n", val.get());
      } else {
        break;
      }
    }
  };

  scheduler.SpawnCoroutine(reader);

  std::this_thread::sleep_for(5s);
  running = false;

  writer.join();
  scheduler.Stop();

  return 0;
}
