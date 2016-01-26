#include "gtest/gtest.h"

#include "async_sleep.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

using namespace boost::chrono;
using namespace std;

class AsyncSleepTest : public ::testing::Test {
public:
  AsyncSleepTest() :
    sleep_task_(io_service_) {}

  boost::asio::io_service io_service_;
  AsyncSleep sleep_task_;
};

TEST_F(AsyncSleepTest, TestSleep) {
  SyncValue<bool> sleep_ran;
  SyncValue<bool> sleep_result;
  auto sleep_func = [this, &sleep_ran, &sleep_result](boost::asio::yield_context context) {
    sleep_ran.SetValue(true);
    sleep_result.SetValue(sleep_task_.Sleep(seconds(1), context));
  };

  boost::asio::spawn(io_service_, move(sleep_func));
  io_service_.run_one();
  auto r = sleep_ran.WaitForValue(seconds(2));
  ASSERT_TRUE(r);
  ASSERT_TRUE(r.get());
  io_service_.run_one();
  auto res = sleep_result.WaitForValue(seconds(2));
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

TEST_F(AsyncSleepTest, TestCancel) {
  SyncValue<bool> sleep_result;
  auto sleep_func = [this, &sleep_result](boost::asio::yield_context context) {
    sleep_result.SetValue(sleep_task_.Sleep(seconds(2), context));
  };

  boost::asio::spawn(io_service_, move(sleep_func));
  io_service_.run_one();
  sleep_task_.Cancel();
  io_service_.run_one();
  auto res = sleep_result.WaitForValue(seconds(2));
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}
