#include "gtest/gtest.h"

#include "scheduler.hpp"
#include "shared_scheduler_context.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <thread>

using namespace std::chrono;
using namespace std;

// Test the multiple-context functionality of th scheduler (via multiple SharedSchedulerContexts)
class SharedSchedulerContextTest : public ::testing::Test {
public:
  SharedSchedulerContextTest() :
    scheduler_ctx_1(parent_scheduler_),
    scheduler_ctx_2(parent_scheduler_) {}

  Scheduler parent_scheduler_;
  SharedSchedulerContext scheduler_ctx_1;
  SharedSchedulerContext scheduler_ctx_2;
};

TEST_F(SharedSchedulerContextTest, TestSpawn) {
  SyncValue<bool> ctx_one_result;
  SyncValue<bool> ctx_one_do_sleep;
  auto ctx_1 = [&](auto context) {
    ctx_one_do_sleep.WaitForValue(100s);
    ctx_one_result.SetValue(scheduler_ctx_1.Sleep(100s, context));
  };
  SyncValue<bool> ctx_two_result;
  SyncValue<bool> ctx_two_do_sleep;
  auto ctx_2 = [&](auto context) {
    ctx_two_do_sleep.WaitForValue(100s);
    ctx_two_result.SetValue(scheduler_ctx_2.Sleep(100ms, context));
  };

  scheduler_ctx_1.SpawnCoroutine(ctx_1);
  scheduler_ctx_2.SpawnCoroutine(ctx_2);
  ctx_one_do_sleep.SetValue(true);
  ctx_two_do_sleep.SetValue(true);
  scheduler_ctx_1.Stop();

  auto res = ctx_one_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());

  res = ctx_two_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

