#pragma once

#include <boost/thread.hpp>
#include <set>

#include "coro_types.h"

class SchedulerInterface;

class CoroHandle {
public:
  CoroHandle(coro_completion_t&& coro_completion, SchedulerInterface* scheduler);
  CoroHandle(CoroHandle&& other);
  void AddTask(const AsyncTaskId& task_id);
  void RemoveTask(const AsyncTaskId& task_id);
  void Join();

//protected:
  coro_completion_t coro_completion_;
  SchedulerInterface* scheduler_;
  boost::mutex tasks_lock_;
  std::set<AsyncTaskId> tasks_; // guarded by tasks_lock_
};
