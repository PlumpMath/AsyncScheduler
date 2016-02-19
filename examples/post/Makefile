CC=g++
CFLAGS=-std=c++1y -g

BOOST_INCLUDE_DIR=~/Downloads/boost-1_56_0_b1
BOOST_LIB_BASE_PATH=$(BOOST_INCLUDE_DIR)/bin.v2/libs
LIB_DIRS=-L $(BOOST_LIB_BASE_PATH)/context/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/coroutine/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/system/build/darwin-4.2.1/debug/link-static/ -L $(BOOST_LIB_BASE_PATH)/thread/build/darwin-4.2.1/debug/link-static/threading-multi -L $(BOOST_LIB_BASE_PATH)/chrono/build/darwin-4.2.1/debug/link-static/
LIBS=-lboost_system -lboost_thread -lboost_coroutine -lboost_context -lboost_chrono

ao: main.cc active_object_datastore.hpp
	$(CC) $(CFLAGS) -I . -I ../../ -I $(BOOST_INCLUDE_DIR) $(LIB_DIRS) $(LIBS) main.cc -o ao
