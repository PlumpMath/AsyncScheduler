#include <cstdlib>

#include "active_object_datastore.hpp"
#include "scheduler.hpp"

#include <boost/thread.hpp>


int main(int argc, char* argv[]) {
  Scheduler scheduler;
  AODataStore<int, int> data(scheduler);
  bool running_ = true;

  boost::thread adder([&]() {
    int i = 0;
    while (running_) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(200));
      data.AddValue(i, i);
      printf("added value %d for key %d\n", i, i);
      i = i >= 10 ? 0 : i + 1; 
    }
  });

  boost::thread reader([&]() {
    int i = 0;
    while (running_) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(300));
      auto val = data.GetValue(i);
      if (val) {
        printf("got value %d for key %d\n", val.get(), i);
      } else {
        printf("no value for key %d\n", i);
      }
      i = i >= 10 ? 0 : i + 1; 
    }
  });

  boost::thread remover([&]() {
    std::srand(std::time(0));
    while (running_) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(600));
      int rand_key = std::rand() % 10;
      printf("removing value for key %d\n", rand_key);
      data.RemoveValue(rand_key);
    }
  });


  boost::this_thread::sleep(boost::posix_time::seconds(10));
  running_ = false;
  adder.join();
  reader.join();
  remover.join();


  return 0;
}
