#include "gtest/gtest.h"

#include "scheduler.hpp"
#include "scheduler_context.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <thread>

using namespace std::chrono;
using namespace std;

// Test the direct functionality of the scheduler (via a single SchedulerContext)
class SchedulerTest : public ::testing::Test {
public:
  SchedulerTest() :
    scheduler_(master_scheduler_) {}

  Scheduler master_scheduler_;
  SchedulerContext scheduler_;
};

TEST_F(SchedulerTest, TestSpawn) {
  SyncValue<bool> func_spawned;
  auto spawn_func = [&](boost::asio::yield_context context) {
    func_spawned.SetValue(true);
  };
  scheduler_.SpawnCoroutine(spawn_func);
  auto res = func_spawned.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

TEST_F(SchedulerTest, TestSpawnMultiple) {
  SyncValue<bool> func1_spawned;
  auto spawn_func1 = [&](auto context) {
    func1_spawned.SetValue(true);
  };
  SyncValue<bool> func2_spawned;
  auto spawn_func2 = [&](auto context) {
    func2_spawned.SetValue(true);
  };
  scheduler_.SpawnCoroutine(spawn_func1);
  scheduler_.SpawnCoroutine(spawn_func2);
  auto res1 = func1_spawned.WaitForValue(1s);
  ASSERT_TRUE(res1);
  ASSERT_TRUE(res1.get());
  auto res2 = func2_spawned.WaitForValue(1s);
  ASSERT_TRUE(res2);
  ASSERT_TRUE(res2.get());
}

// Call spawn after the scheduler has been stopped
TEST_F(SchedulerTest, TestSpawnStopped) {
  SyncValue<bool> func_spawned;
  auto spawn_func = [&](boost::asio::yield_context context) {
    func_spawned.SetValue(true);
  };
  scheduler_.Stop();
  scheduler_.SpawnCoroutine(spawn_func);
  auto res = func_spawned.WaitForValue(10ms);
  ASSERT_FALSE(res);
}

// Test async sleep
TEST_F(SchedulerTest, TestAsyncSleep) {
  SyncValue<bool> sleep_result;
  auto sleeper = [&](boost::asio::yield_context context) {
    sleep_result.SetValue(scheduler_.Sleep(10ms, context));
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

TEST_F(SchedulerTest, TestPostAsyncFuture) {
  auto do_stuff = []() {
    return 42;
  };

  SyncValue<int> result;
  auto coro = [&](auto context) {
    auto future = scheduler_.Post(do_stuff, UseAsync);
    auto res = future->Get(context);
    ASSERT_TRUE(res);
    result.SetValue(res.get());
  };

  scheduler_.SpawnCoroutine(coro);
  auto res = result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_EQ(42, res.get());
}

// Make sure that, for an object with all the properly defined
//  move operations, we don't copy more than necessary
class MovableObj {
public:
  MovableObj(int num) : num_(num) {}
  MovableObj(MovableObj& other) {
    num_ = other.num_;
    num_copies_ = other.num_copies_ + 1;
  }
  MovableObj(const MovableObj& other) {
    num_ = other.num_;
    num_copies_ = other.num_copies_ + 1;
  }
  MovableObj(MovableObj&& other) {
    num_ = other.num_;
    num_copies_ = other.num_copies_;
  }

  int num_ = 0;
  int num_copies_ = 0;
};

TEST_F(SchedulerTest, TestVoidPostNumCopies) {
  SyncValue<int> num_copies;
  MovableObj obj(42);
  scheduler_.Post([o = std::move(obj), &num_copies]() {
    num_copies.SetValue(o.num_copies_);
  });
  auto res = num_copies.WaitForValue(2s);
  ASSERT_TRUE(res);
  ASSERT_EQ(1, res.get());
}

TEST_F(SchedulerTest, TestRetValuePostNumCopies) {
  SyncValue<int> num_copies;
  MovableObj obj(42);
  scheduler_.Post([o = std::move(obj), &num_copies]() {
    num_copies.SetValue(o.num_copies_);
    return true;
  });
  auto res = num_copies.WaitForValue(2s);
  ASSERT_TRUE(res);
  ASSERT_EQ(1, res.get());
}

TEST_F(SchedulerTest, TestRetValueAsyncPostNumCopies) {
  SyncValue<int> num_copies;
  MovableObj obj(42);
  scheduler_.Post([o = std::move(obj), &num_copies]() {
    num_copies.SetValue(o.num_copies_);
    return true;
  }, UseAsync);
  auto res = num_copies.WaitForValue(2s);
  ASSERT_TRUE(res);
  ASSERT_EQ(1, res.get());
}

// Test stopping the scheduler while a posted function is running
// TODO: technically it's possible here that the posted function
//  finishes before we ever call Stop.  it's a bit tricky to guarantee
//  that the posted function is in the middle of running while we call
//  stop. added a long sleep to try and make it very likely that the func
//  is still running while calling stop, but still isn't totally deterministic
//  and it slows the tests down a lot
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

TEST_F(SchedulerTest, TestSemaphore) {
  SyncValue<bool> sem_result;
  auto semaphore_handle = scheduler_.CreateSemaphore();
  auto func = [&](boost::asio::yield_context context) {
    sem_result.SetValue(scheduler_.WaitOnSemaphore(semaphore_handle, context));
  };

  scheduler_.SpawnCoroutine(func);
  auto res = sem_result.WaitForValue(100ms);
  ASSERT_FALSE(res);
  scheduler_.RaiseSemaphore(semaphore_handle);
  res = sem_result.WaitForValue(100ms);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

TEST_F(SchedulerTest, TestCreateSemaphoreFromSchedulerThread) {
  auto wait = [&](auto sem_handle) {
    scheduler_.RaiseSemaphore(sem_handle);
  };
  SyncValue<bool> sem_wait_finished;
  auto sem_creator = [&](auto context) {
    auto sem_handle = scheduler_.CreateSemaphore();
    std::async(std::launch::async, wait, sem_handle);
    scheduler_.WaitOnSemaphore(sem_handle, context);
    sem_wait_finished.SetValue(true);
  };

  scheduler_.SpawnCoroutine(sem_creator);
  auto res = sem_wait_finished.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_TRUE(*res);
}

TEST_F(SchedulerTest, TestStopWhileWaitingOnSemaphore) {
  SyncValue<bool> coro_run;
  SyncValue<bool> sem_result;
  auto semaphore_handle = scheduler_.CreateSemaphore();
  auto func = [&](boost::asio::yield_context context) {
    coro_run.SetValue(true);
    sem_result.SetValue(scheduler_.WaitOnSemaphore(semaphore_handle, context));
  };

  scheduler_.SpawnCoroutine(func);
  ASSERT_TRUE(coro_run.WaitForValue(1s));
  scheduler_.Stop();
  auto res = sem_result.WaitForValue(10ms);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}

TEST_F(SchedulerTest, TestCreateFutureFromNonSchedulerThread) {
  SyncValue<int> fut_result;
  auto future = scheduler_.CreateFuture<int>();
  auto waiter = [&](boost::asio::yield_context context) {
    auto result = future->Get(context);
    if (result) {
      fut_result.SetValue(result.get());
    }
  };
  std::async(std::launch::async, [&]() {
    int x = 0;
    for (int i = 0; i < 1000; ++i) {
      x += i;
    }
    future->SetValue(x);
  });
  scheduler_.SpawnCoroutine(waiter);
  auto res = fut_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_EQ(499500, res.get());
}

TEST_F(SchedulerTest, TestCreateFutureFromSchedulerThread) {
  SyncValue<int> fut_result;
  auto do_work =[&](auto future) {
    int x = 0;
    for (int i = 0; i < 1000; ++i) {
      x += i;
    }
    future->SetValue(x);
  };
  auto waiter = [&](boost::asio::yield_context context) {
    auto future = scheduler_.CreateFuture<int>();
    std::async(std::launch::async, do_work, future);
    auto res = future->Get(context);
    if (res) {
      fut_result.SetValue(res.get());
    }
  };

  scheduler_.SpawnCoroutine(waiter);
  auto res = fut_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_EQ(499500, res.get());
}

TEST_F(SchedulerTest, TestStopSchedulerWhileWaitingOnFuture) {
  SyncValue<bool> coro_started;
  SyncValue<bool> got_future_result;
  auto future = scheduler_.CreateFuture<int>();
  auto waiter = [&](boost::asio::yield_context context) {
    coro_started.SetValue(true);
    auto result = future->Get(context);
    got_future_result.SetValue(!!result);
  };

  scheduler_.SpawnCoroutine(waiter);
  ASSERT_TRUE(coro_started.WaitForValue(1s));
  scheduler_.Stop();
  auto res = got_future_result.WaitForValue(1s);
  ASSERT_TRUE(res);
  ASSERT_FALSE(res.get());
}

// Stop the scheduler while it has active sleeps, semaphores, futures and coroutines
TEST_F(SchedulerTest, TestStopWithActiveOperations) {
  SyncValue<bool> semaphore_wait_done;
  auto semaphore_handle = scheduler_.CreateSemaphore();
  auto semaphore_waiter = [&](auto context) {
    scheduler_.WaitOnSemaphore(semaphore_handle, context);
    semaphore_wait_done.SetValue(true);
  };

  SyncValue<bool> sleep_done;
  auto sleeper = [&](auto context) {
    scheduler_.Sleep(100s, context);
    sleep_done.SetValue(true);
  };

  SyncValue<bool> future_wait_done;
  auto future = scheduler_.CreateFuture<int>();
  auto future_waiter = [&](auto context) {
    auto result = future->Get(context);
    future_wait_done.SetValue(true);
  };

  // this long sleep slows down the tests, but it's an attempt
  //  to workaround the fact that we can't really guarantee this
  //  will still be running by the time we call stop
  auto sync_post_waiter = [&]() {
    auto future = scheduler_.Post([]() {
      std::this_thread::sleep_for(2s);
      return 42;
    });
    ASSERT_EQ(42, future.get());
  };

  auto async_post_waiter = [&](auto context) {
    auto future = scheduler_.Post([]() {
      std::this_thread::sleep_for(2s);
      return 42;
    }, UseAsync);
    auto res = future->Get(context);
    ASSERT_TRUE(res);
    ASSERT_EQ(42, *res);
  };

  scheduler_.SpawnCoroutine(semaphore_waiter);
  scheduler_.SpawnCoroutine(sleeper);
  scheduler_.SpawnCoroutine(future_waiter);
  std::async(std::launch::async, sync_post_waiter);

  scheduler_.Stop();
  auto res_sem = semaphore_wait_done.WaitForValue(1s);
  ASSERT_TRUE(res_sem);
  ASSERT_TRUE(res_sem.get());

  auto res_sleep = sleep_done.WaitForValue(1s);
  ASSERT_TRUE(res_sleep);
  ASSERT_TRUE(res_sleep.get());

  auto res_fut = future_wait_done.WaitForValue(1s);
  ASSERT_TRUE(res_fut);
  ASSERT_TRUE(res_fut.get());
}
