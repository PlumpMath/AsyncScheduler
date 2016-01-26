#include "gtest/gtest.h"

#include "scheduler_interface.h"
#include "scheduler.hpp"
#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

using namespace bc;
using namespace std;

class SchedulerTest : public ::testing::Test {
public:
  SchedulerTest() : scheduler_(make_shared<Scheduler>()) {}
  
  shared_ptr<SchedulerInterface> scheduler_;
};

TEST_F(SchedulerTest, TestSpawn) {
  SyncValue<bool> coro_spawned;
  auto coro = [&](YieldContext& context) {
    coro_spawned.SetValue(true);
  };
  coro_completion_t coro_done = scheduler_->Spawn(std::move(coro));

  auto res = coro_spawned.WaitForValue(seconds(2));
  ASSERT_TRUE(res.value_or(false));

  ASSERT_EQ(future_status::ready, coro_done.wait_for(std::chrono::seconds(10)));
  ASSERT_TRUE(coro_done.get());
}

TEST_F(SchedulerTest, TestSleep) {
  SyncValue<bool> sleep_done;
  auto coro = [&](YieldContext& context) {
    scheduler_->Sleep(milliseconds(100), context);
    sleep_done.SetValue(true);
  };

  coro_completion_t coro_done = scheduler_->Spawn(std::move(coro));
  auto res = sleep_done.WaitForValue(seconds(2));
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());

  ASSERT_EQ(future_status::ready, coro_done.wait_for(std::chrono::seconds(10)));
  ASSERT_TRUE(coro_done.get());
}

TEST_F(SchedulerTest, TestWaitOnEvent) {
  AsyncTaskId event_id = scheduler_->AddEvent();
  SyncValue<bool> coro_started;
  SyncValue<bool> wakeup_result; 
  auto coro = [&](YieldContext& context) {
    coro_started.SetValue(true);
    wakeup_result.SetValue(scheduler_->WaitOnEvent(event_id, context));
  };

  coro_completion_t coro_done = scheduler_->Spawn(std::move(coro));
  auto r = coro_started.WaitForValue(seconds(2));
  ASSERT_TRUE(r);
  ASSERT_TRUE(r.get());
  scheduler_->FireEvent(event_id);
  auto res = wakeup_result.WaitForValue(seconds(3));
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());

  ASSERT_EQ(future_status::ready, coro_done.wait_for(std::chrono::seconds(10)));
  ASSERT_TRUE(coro_done.get());
}

TEST_F(SchedulerTest, TestWaitOnNonExistentEvent) {
  SyncValue<bool> coro_started;
  SyncValue<bool> wakeup_result; 
  auto coro = [&](YieldContext& context) {
    coro_started.SetValue(true);
    wakeup_result.SetValue(scheduler_->WaitOnEvent(999, context));
  };

  coro_completion_t coro_done = scheduler_->Spawn(std::move(coro));
  auto r = coro_started.WaitForValue(seconds(2));
  ASSERT_TRUE(r);
  ASSERT_TRUE(r.get());
  auto res = wakeup_result.WaitForValue(seconds(3));
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());

  ASSERT_EQ(future_status::ready, coro_done.wait_for(std::chrono::seconds(10)));
  ASSERT_TRUE(coro_done.get());
}


TEST_F(SchedulerTest, TestCancelEvent) {
  AsyncTaskId event_id = scheduler_->AddEvent();
  SyncValue<bool> coro_started;
  SyncValue<bool> wakeup_result;
  std::promise<bool> event_wakeup;
  auto coro = [&](YieldContext& context) {
    coro_started.SetValue(true);
    wakeup_result.SetValue(scheduler_->WaitOnEvent(event_id, context));
  };

  coro_completion_t coro_done = scheduler_->Spawn(std::move(coro));
  auto r = coro_started.WaitForValue(seconds(2));
  ASSERT_TRUE(r);
  ASSERT_TRUE(r.get());
  scheduler_->CancelTask(event_id);
  auto res = wakeup_result.WaitForValue(seconds(2));
  // 'Cancel' should give us a 'false' result
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());

  ASSERT_EQ(future_status::ready, coro_done.wait_for(std::chrono::seconds(10)));
  ASSERT_TRUE(coro_done.get());
}

// Class that mimics a class that would be given a scheduler
class ClassWithCoroutine {
public:
  ClassWithCoroutine(shared_ptr<SchedulerInterface>& scheduler) :
      scheduler_(scheduler) {
    coro_task_id_ = scheduler_->AddEvent();
  }

  ~ClassWithCoroutine() {
  }

  void Coro(YieldContext& context) {
    while (true) {
      coro_started_.SetValue(true);
      if (!scheduler_->WaitOnEvent(coro_task_id_, context)) {
        // our timer has been cancelled, time to quit!
        break;
      }
      // do something based on the event happening
    }
  }

  void Start() {
    coro_done_ = scheduler_->Spawn(boost::bind(&ClassWithCoroutine::Coro, this, _1));
  }

  void Stop() {
    scheduler_->CancelTask(coro_task_id_);
    coro_done_.get();
  }

  shared_ptr<SchedulerInterface> scheduler_;
  AsyncTaskId coro_task_id_;
  coro_completion_t coro_done_;
  SyncValue<bool> coro_started_;
};

TEST_F(SchedulerTest, TestCleanupClassWaitingOnEvent) {
  {
    ClassWithCoroutine foo(scheduler_);
    foo.Start();
    foo.coro_started_.WaitForValue(seconds(2));
    foo.Stop();
  }

  weak_ptr<SchedulerInterface> scheduler_w = scheduler_;
  scheduler_.reset();
  EXPECT_TRUE(scheduler_w.use_count() == 0 && scheduler_w.lock() == nullptr);
}
