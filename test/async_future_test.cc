#include "gtest/gtest.h"

#include "async_future.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>

using namespace std::chrono;
using namespace std;

class AsyncFutureTest : public ::testing::Test {
public:
  AsyncFutureTest() :
    async_future_(io_service_) {}

  boost::asio::io_service io_service_;
  AsyncFuture<int> async_future_;
};

// Wait on a value, then set it
TEST_F(AsyncFutureTest, TestGetAndSet) {
  SyncValue<boost::optional<int>> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_future_.Get(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  // Run once to spawn the coroutine
  io_service_.run_one();
  async_future_.SetValue(42);
  // Let the SetValue execute and the coro resume
  io_service_.run();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  boost::optional<int> value = res.get();
  ASSERT_TRUE(value);
  ASSERT_EQ(42, value.get());
}

// Get a value that was already set
TEST_F(AsyncFutureTest, TestSetAndGet) {
  SyncValue<boost::optional<int>> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_future_.Get(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  async_future_.SetValue(42);
  io_service_.run();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  boost::optional<int> value = res.get();
  ASSERT_TRUE(value);
  ASSERT_EQ(42, value.get());
}

// Cancel a future while it's being waited on
TEST_F(AsyncFutureTest, TestCancel) {
  SyncValue<boost::optional<int>> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_future_.Get(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  io_service_.run_one();
  async_future_.Cancel();
  io_service_.run();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  boost::optional<int> value = res.get();
  ASSERT_FALSE(value);
}

// Try getting a cancelled future
TEST_F(AsyncFutureTest, TestGetCancelled) {
  SyncValue<boost::optional<int>> wait_result;
  auto wait_func = [&](boost::asio::yield_context context) {
    wait_result.SetValue(async_future_.Get(context));
  };
  boost::asio::spawn(io_service_, move(wait_func));
  async_future_.Cancel();
  io_service_.run();
  auto res = wait_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  boost::optional<int> value = res.get();
  ASSERT_FALSE(value);
}
