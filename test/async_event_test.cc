#include "gtest/gtest.h"

#include "async_event.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "sync_value.hpp"

using namespace boost::chrono;
using namespace std;

class AsyncEventTest : public ::testing::Test {
public:
  AsyncEventTest() :
    event_(io_service_) {}

  boost::asio::io_service io_service_;
  AsyncEvent event_;
};

TEST_F(AsyncEventTest, TestNotify) {
  SyncValue<bool> event_result;
  auto wait_func = [this, &event_result](boost::asio::yield_context context) {
    event_result.SetValue(event_.Wait(context));
  };

  boost::asio::spawn(io_service_, move(wait_func));
  io_service_.run_one();
  event_.Notify();
  io_service_.run_one();

  auto res = event_result.WaitForValue(seconds(2));
  ASSERT_TRUE(res);
  ASSERT_EQ(true, res.get());
}

TEST_F(AsyncEventTest, TestNotifyBeforeWait) {
  SyncValue<bool> event_result;
  auto coro = [this, &event_result](boost::asio::yield_context context) {
    event_result.SetValue(event_.Wait(context));
  };
  std::unique_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io_service_));

  boost::thread thread(boost::bind(&boost::asio::io_service::run, &io_service_));

  event_.Notify();
  boost::asio::spawn(io_service_, move(coro));
  auto res = event_result.WaitForValue(seconds(4));
  ASSERT_TRUE(res);
  ASSERT_EQ(true, res.get());

  work.reset();
  thread.join();
}

TEST_F(AsyncEventTest, TestMultipleNotify) {
  SyncValue<bool> event_result;
  int num_events = 3;
  auto coro = [this, &event_result, num_events](boost::asio::yield_context context) {
    int num_events_seen = 0;
    while (num_events_seen < num_events) {
      event_.Wait(context);
      ++num_events_seen;
    }
    event_result.SetValue(true);
  };
  std::unique_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io_service_));

  boost::thread thread(boost::bind(&boost::asio::io_service::run, &io_service_));
  boost::asio::spawn(io_service_, move(coro));

  for (int i = 0; i < num_events; ++i) {
    event_.Notify();
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
  }
  auto res = event_result.WaitForValue(seconds(4));
  ASSERT_TRUE(res);
  ASSERT_EQ(true, res.get());

  work.reset();
  thread.join();
}

TEST_F(AsyncEventTest, TestCancel) {
  SyncValue<bool> event_result;
  auto wait_func = [this, &event_result](boost::asio::yield_context context) {
    event_result.SetValue(event_.Wait(context));
  };

  boost::asio::spawn(io_service_, move(wait_func));
  io_service_.run_one();
  event_.Cancel();
  io_service_.run_one();

  auto res = event_result.WaitForValue(seconds(2));
  ASSERT_TRUE(res);
  ASSERT_EQ(false, res.get());
}

TEST_F(AsyncEventTest, TestWaitOnCancelledEvent) {
  SyncValue<bool> event_result;
  auto coro = [&](boost::asio::yield_context context) {
    event_result.SetValue(event_.Wait(context));
  };

  event_.Cancel();
  boost::asio::spawn(io_service_, move(coro));
  io_service_.run();
  auto res = event_result.WaitForValue(seconds(2));
  ASSERT_TRUE(res);
  ASSERT_EQ(false, res.get());
}
