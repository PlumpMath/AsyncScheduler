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
  AsyncExecutorTest() :
    executor_(io_service_) {}

  boost::asio::io_service io_service_;
  AsyncExecutor executor_;
};

TEST_F(AsyncExecutorTest, TestRun) {
  auto func = [&]() {
    return 42;
  };
  auto run_res = executor_.Run(func);
  io_service_.run_one();
  ASSERT_TRUE(run_res);
  auto run_fut = std::move(run_res.get());
  ASSERT_EQ(42, run_fut.get());
}

TEST_F(AsyncExecutorTest, TestCancel) {
  auto func = [&]() {
    return 42;
  };
  executor_.Cancel();
  auto run_res = executor_.Run(func);
  ASSERT_FALSE(run_res);
}

TEST_F(AsyncExecutorTest, TestDifferentType) {
  auto func = [&]() {
    return "42";
  };
  auto run_res = executor_.Run(func);
  io_service_.run_one();
  ASSERT_TRUE(run_res);
  auto run_fut = std::move(run_res.get());
  ASSERT_TRUE(strncmp("42", run_fut.get(), 2) == 0);
}

class Foo {
public:
  bool doBool(bool v) {
    return v;
  }
  
  int doInt(int v) {
    return v;
  }
};

TEST_F(AsyncExecutorTest, TestMemberFunction) {
  Foo foo;
  auto run_res = executor_.Run(boost::bind(&Foo::doBool, &foo, false));
  io_service_.run_one();
  ASSERT_TRUE(run_res);
  auto run_fut = std::move(run_res.get());
  ASSERT_EQ(false, run_fut.get());
}

TEST_F(AsyncExecutorTest, TestMemberFunction2) {
  Foo foo;
  auto run_res = executor_.Run(boost::bind(&Foo::doInt, &foo, 42));
  io_service_.run_one();
  ASSERT_TRUE(run_res);
  auto run_fut = std::move(run_res.get());
  ASSERT_EQ(42, run_fut.get());
}
