CC=g++
CFLAGS=-std=c++1y -g

BOOST_INCLUDE_DIR=~/Downloads/boost-1_56_0_b1
BOOST_LIB_BASE_PATH=$(BOOST_INCLUDE_DIR)/bin.v2/libs
LIB_DIRS=-L $(BOOST_LIB_BASE_PATH)/context/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/coroutine/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/system/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/thread/build/darwin-4.2.1/debug/link-static/threading-multi -L $(BOOST_LIB_BASE_PATH)/chrono/build/darwin-4.2.1/debug/link-static/
LIBS=-lboost_system -lboost_thread -lboost_coroutine -lboost_context -lboost_chrono

GTEST_ROOT=./googletest/googletest
GMOCK_ROOT=./googletest/googlemock

async_sleep_test: test/async_sleep_test.cc
	$(CC) $(CFLAGS) -I $(GTEST_ROOT)/include -I . -I $(BOOST_INCLUDE_DIR) $(LIB_DIRS) -L $(GMOCK_ROOT)/gtest -lgtest -lgtest_main $(LIBS) test/async_sleep_test.cc -o test/async_sleep_test

async_semaphore_test: test/async_semaphore_test.cc
	$(CC) $(CFLAGS) -I $(GTEST_ROOT)/include -I . -I $(BOOST_INCLUDE_DIR) $(LIB_DIRS) -L $(GMOCK_ROOT)/gtest -lgtest -lgtest_main $(LIBS) test/async_semaphore_test.cc -o test/async_semaphore_test

async_future_test: test/async_future_test.cc
	$(CC) $(CFLAGS) -I $(GTEST_ROOT)/include -I . -I $(BOOST_INCLUDE_DIR) $(LIB_DIRS) -L $(GMOCK_ROOT)/gtest -lgtest -lgtest_main $(LIBS) test/async_future_test.cc -o test/async_future_test

scheduler_test: test/scheduler_test.cc
	$(CC) $(CFLAGS) -I $(GTEST_ROOT)/include -I . -I $(BOOST_INCLUDE_DIR) $(LIB_DIRS) -L $(GMOCK_ROOT)/gtest -lgtest -lgtest_main $(LIBS) test/scheduler_test.cc -o test/scheduler_test

async_executor_test: test/async_executor_test.cc
	$(CC) $(CFLAGS) -I $(GTEST_ROOT)/include -I . -I $(BOOST_INCLUDE_DIR) $(LIB_DIRS) -L $(GMOCK_ROOT)/gtest -lgtest -lgtest_main $(LIBS) test/async_executor_test.cc -o test/async_executor_test
