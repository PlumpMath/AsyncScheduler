#include "coro_handle.h"
#include "scheduler_interface.h"

CoroHandle::CoroHandle(coro_completion_t&& coro_completion, SchedulerInterface* scheduler) :
    coro_completion_(std::move(coro_completion)), scheduler_(scheduler) {
}

CoroHandle::CoroHandle(CoroHandle&& other) :
  coro_completion_(std::move(other.coro_completion_)), scheduler_(other.scheduler_) {
}

void CoroHandle::AddTask(const AsyncTaskId& task_id) {
  boost::lock_guard<boost::mutex> guard(tasks_lock_);
  printf("coro handle adding task %d\n", task_id);
	tasks_.insert(task_id);
}

void CoroHandle::RemoveTask(const AsyncTaskId& task_id) {
  boost::lock_guard<boost::mutex> guard(tasks_lock_);
  printf("coro handle erasing task %d\n", task_id);
	tasks_.erase(task_id);
}

void CoroHandle::Join() {
  // this is a problem...if a coro calls add after we cancel everything
  //  we saw, it'll deadlock. we could do a flag (protected by a second
  //  lock) that says whether or not we're joining?  this method
  //  would acquire the flag lock, set it, then release that lock
  //  addtask would have to acquire both the flag lock (and check it)
  //  then (while still holding it) acquire the task lock
  //  if the flag was set, it would release that lock and return a
  //  value that told the caller to cancel.
  //  ...feeling pretty awkward
  boost::lock_guard<boost::mutex> guard(tasks_lock_);
  printf("coro handle joining\n");
  for (auto &t : tasks_) {
    printf("coro handle cancelling task %d\n", t);
    scheduler_->CancelTask(t);
  }
  coro_completion_.wait();
}
