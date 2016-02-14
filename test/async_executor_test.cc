#include "gtest/gtest.h"

#include "async_executor.hpp"

#include "sync_value.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>

using namespace std::chrono;
using namespace std;

class AsyncExecutorTest : public ::testing::Test {
public:
  boost::asio::io_service io_service_;
};

TEST_F(AsyncExecutorTest, TestRun) {
  SyncValue<bool> func_run;
  auto func = [&]() {
    func_run.SetValue(true);
  };
  AsyncExecutor executor(io_service_, func);
  auto run_res = executor.Run();
  io_service_.run_one();
  ASSERT_TRUE(run_res.get());
  auto res = func_run.WaitForValue(10ms);
  ASSERT_TRUE(res);
  ASSERT_TRUE(res.get());
}

TEST_F(AsyncExecutorTest, TestCancel) {
  SyncValue<bool> func_run;
  auto func = [&]() {
    func_run.SetValue(true);
  };
  AsyncExecutor executor(io_service_, func);
  executor.Cancel();
  auto run_res = executor.Run();
  io_service_.run_one();
  ASSERT_FALSE(run_res.get());
  auto res = func_run.WaitForValue(10ms);
  ASSERT_FALSE(res);
}
