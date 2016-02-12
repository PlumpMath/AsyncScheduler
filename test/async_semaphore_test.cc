#include "gtest/gtest.h"

#include "async_semaphore.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>

#include <functional> // std::bind

using namespace std::chrono;
using namespace std;

class AsyncSemaphoreTest : public ::testing::Test {
public:
  AsyncSemaphoreTest() :
    async_semaphore_(io_service_) {}

  boost::asio::io_service io_service_;
  AsyncSemaphore async_semaphore_;
};

// Wait on the semaphore, raise it, make sure we get a proper result
TEST_F(AsyncSemaphoreTest, TestWaitAndRaise) {
  SyncValue<bool> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_semaphore_.Wait(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  io_service_.run_one();
  async_semaphore_.Raise();
  io_service_.run_one();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

// Wait on a semaphore that was already raised
TEST_F(AsyncSemaphoreTest, TestWaitOnAlreadyRaised) {
  SyncValue<bool> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_semaphore_.Wait(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  async_semaphore_.Raise();
  // The call to Wait should return immediately here, so we
  //  should only need to call run_one once
  io_service_.run_one();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

// Raise a semaphore twice, make sure both waits return immediately
TEST_F(AsyncSemaphoreTest, TestRaiseTwiceWaitTwice) {
  SyncValue<bool> wait_result_one;
  SyncValue<bool> wait_result_two;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result_one.SetValue(async_semaphore_.Wait(context));
    wait_result_two.SetValue(async_semaphore_.Wait(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  async_semaphore_.Raise();
  async_semaphore_.Raise();
  // The calls to Wait should return immediately here, so we
  //  should only need to call run_one once
  io_service_.run_one();
  auto res = wait_result_one.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
  res = wait_result_two.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

// Wait twice, raise one at a time
TEST_F(AsyncSemaphoreTest, TestWaitTwiceRaiseTwice) {
  SyncValue<bool> wait_result_one;
  SyncValue<bool> wait_result_two;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result_one.SetValue(async_semaphore_.Wait(context));
    wait_result_two.SetValue(async_semaphore_.Wait(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  io_service_.run_one();
  async_semaphore_.Raise();
  io_service_.run_one();
  auto res = wait_result_one.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
  res = wait_result_two.WaitForValue(10ms);
  // It shouldn't have completed yet
  ASSERT_FALSE(res);
  async_semaphore_.Raise();
  io_service_.run_one();
  res = wait_result_two.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

// Cancel a semaphore while it's being waited on
TEST_F(AsyncSemaphoreTest, TestCancel) {
  SyncValue<bool> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_semaphore_.Wait(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  io_service_.run_one();
  async_semaphore_.Cancel();
  io_service_.run_one();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}

// Wait on an already-cancelled semaphore
TEST_F(AsyncSemaphoreTest, TestWaitOnCancelled) {
  SyncValue<bool> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_semaphore_.Wait(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  async_semaphore_.Cancel();
  io_service_.run_one();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}
