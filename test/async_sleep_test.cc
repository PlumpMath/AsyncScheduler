#include "gtest/gtest.h"

#include "async_sleep.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>

#include <functional> // std::bind

using namespace std::chrono;
using namespace std;

class AsyncSleepTest : public ::testing::Test {
public:
  AsyncSleepTest() :
    async_sleep_(io_service_) {}

  boost::asio::io_service io_service_;
  AsyncSleep async_sleep_;
};

TEST_F(AsyncSleepTest, TestSleep) {
  SyncValue<bool> sleep_result;
  auto sleep_func = [&](boost::asio::yield_context context) {
    sleep_result.SetValue(async_sleep_.Sleep(10ms, context));
  };

  boost::asio::spawn(io_service_, move(sleep_func));
  io_service_.run();
  auto res = sleep_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

TEST_F(AsyncSleepTest, TestCancel) {
  SyncValue<bool> sleep_result;
  auto sleep_func = [&](boost::asio::yield_context context) {
    sleep_result.SetValue(async_sleep_.Sleep(100s, context));
  };

  boost::asio::spawn(io_service_, move(sleep_func));
  // This 'run_one' will spawn the coroutine and do the sleep.
  //  Since the sleep is asynchronous, once it enters the sleep
  //  it'll return control back to us (the test), so we know
  //  the coroutine is sleeping when this first run_one is done
  io_service_.run_one();
  async_sleep_.Cancel();
  // run the io_service again to allow the sleep to see it's been
  //  cancelled and wake up
  io_service_.run_one();
  auto res = sleep_result.WaitForValue(2s);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}

TEST_F(AsyncSleepTest, TestSleepOnCancelledSleepEvent) {
  SyncValue<bool> sleep_result;
  auto sleep_func = [this, &sleep_result](boost::asio::yield_context context) {
    sleep_result.SetValue(async_sleep_.Sleep(2s, context));
  };

  async_sleep_.Cancel();
  boost::asio::spawn(io_service_, move(sleep_func));
  io_service_.run();
  auto res = sleep_result.WaitForValue(2s);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}
