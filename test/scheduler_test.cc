#include "gtest/gtest.h"

#include "scheduler.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>

using namespace std::chrono;
using namespace std;

class SchedulerTest : public ::testing::Test {
public:
  Scheduler scheduler_;
};

TEST_F(SchedulerTest, TestSpawn) {
  SyncValue<bool> func_spawned;
  auto spawn_func = [&](boost::asio::yield_context context) {
    printf("spawned\n");
    func_spawned.SetValue(true);
  };
  scheduler_.SpawnCoroutine(spawn_func);
  auto res = func_spawned.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

// Call spawn after the scheduler has been stopped
TEST_F(SchedulerTest, TestSpawnStopped) {
  SyncValue<bool> func_spawned;
  auto spawn_func = [&](boost::asio::yield_context context) {
    printf("spawned\n");
    func_spawned.SetValue(true);
  };
  scheduler_.Stop();
  scheduler_.SpawnCoroutine(spawn_func);
  auto res = func_spawned.WaitForValue(1s);
  ASSERT_FALSE(res);
}

// Test async sleep
TEST_F(SchedulerTest, TestAsyncSleep) {
  SyncValue<bool> sleep_result;
  auto sleeper = [&](boost::asio::yield_context context) {
    sleep_result.SetValue(scheduler_.Sleep(100ms, context));
  };
  scheduler_.SpawnCoroutine(sleeper);
  auto res = sleep_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

// Test stopping the scheduler while sleeping
TEST_F(SchedulerTest, TestStopWhileSleeping) {
  SyncValue<bool> spawned;
  SyncValue<bool> sleep_result;
  auto sleeper = [&](boost::asio::yield_context context) {
    spawned.SetValue(true);
    sleep_result.SetValue(scheduler_.Sleep(100s, context));
  };
  scheduler_.SpawnCoroutine(sleeper);
  ASSERT_TRUE(spawned.WaitForValue(1s));
  scheduler_.Stop();
  auto res = sleep_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}

TEST_F(SchedulerTest, TestPost) {
  auto func = []() {
    return 42;
  };
  auto res = scheduler_.Post(func);
  ASSERT_EQ(42, res.get());
}

TEST_F(SchedulerTest, TestPostVoidFunc) {
  SyncValue<bool> func_ran;
  auto func = [&]() {
    func_ran.SetValue(true);
  };
  scheduler_.Post(func);
  auto res = func_ran.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

// Test stopping the scheduler while a posted function is running
// TODO: technically it's possible here that the posted function
//  finishes before we ever call Stop.  it's a bit tricky to guarantee
//  that the posted function is in the middle of running while we call
//  stop.
TEST_F(SchedulerTest, TestStopWhilePosting) {
  SyncValue<bool> post_func_running;
  SyncValue<bool> post_func_ran;
  auto func = [&]() {
    post_func_running.SetValue(true);
    std::this_thread::sleep_for(4s);
    post_func_ran.SetValue(true);
    return 42;
  };
  scheduler_.Post(func);
  ASSERT_TRUE(post_func_running.WaitForValue(1s));
  scheduler_.Stop();
  ASSERT_TRUE(post_func_ran.WaitForValue(1s));
}
