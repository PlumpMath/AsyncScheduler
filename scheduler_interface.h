#pragma once

#include <future>

#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <boost/function.hpp>
#include <boost/asio/spawn.hpp>

#include "coro_handle.h"
#include "coro_types.h"

typedef boost::function<void(boost::asio::yield_context, CoroHandle&)> CoroFunc;
typedef boost::asio::yield_context YieldContext;

class SchedulerInterface {
public:

  virtual ~SchedulerInterface() {}
  // Spawns the given function as a coroutine
  // @return a future that is completed when the spawned coroutine exits 
  virtual CoroHandle Spawn(CoroFunc&& func) = 0;
  // @return true if the sleep completed successfully, false if it was forcibly awoken early
  // NOTE: No handle to a sleep task is given here (because then sleep would involve two calls:
  //  AddSleepEvent and Sleep, which feels odd), so a sleep cannot be cancelled independently.
  //  TODO: It is only cancelled when cleaning up an entire context.
  virtual bool Sleep(
    const boost::chrono::duration<double>& duration, 
    YieldContext& context,
    CoroHandle& handle) = 0;
  // @return true if the waited on event fired, false if it was cancelled
  virtual bool WaitOnEvent(const AsyncTaskId& event_id, YieldContext& context) = 0;
  // Creates a new AsyncEvent
  // @return a handle to the created event
  virtual AsyncTaskId AddEvent(CoroHandle& handle) = 0;
  virtual void FireEvent(const AsyncTaskId& event_id) = 0;
  // Cancels the given task and deletes it
  virtual void CancelTask(const AsyncTaskId& task_id) = 0;
};
