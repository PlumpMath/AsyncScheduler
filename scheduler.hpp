#pragma once

#include "scheduler_interface.h"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/ref.hpp>

#include <memory>

#include "async_sleep.hpp"
#include "async_event.hpp"

class Scheduler : public SchedulerInterface {
public:
  Scheduler() :
    work_(new boost::asio::io_service::work(io_service_)),
    thread_(boost::bind(&boost::asio::io_service::run, &io_service_)) {
  }
  virtual ~Scheduler() {
    //NOTE: scheduler should always outlive any object using it.  All users of the scheduler
    // should be cleaned up properly.  Since they all have shared_ptrs to the Scheduler, by the
    // time this is called, all coroutines should be completed
    //std::cout << "Scheduler dtor running in thread " << boost::this_thread::get_id() << std::endl;
    io_service_.stop();
    thread_.join();
    //std::cout << "Scheduler dtor done\n";
  }

  virtual CoroHandle Spawn(CoroFunc&& func) override {
    std::promise<bool> promise;
    auto coro_finished_future = promise.get_future();
    CoroHandle handle(std::move(coro_finished_future), this);
    auto coro_func = [p = std::move(promise), func, &handle](const YieldContext& context) mutable {
      func(context, handle);
      //std::cout << "(Scheduler): coro finished, completing future\n";
      p.set_value(true);
    };
    boost::asio::spawn(io_service_, std::move(coro_func));
    //return coro_finished_future;
    return handle;
  }

  virtual bool Sleep(
      const boost::chrono::duration<double>& duration, 
      YieldContext& context,
      CoroHandle& handle) override {
    std::shared_ptr<AsyncSleep> sleep_task = std::make_shared<AsyncSleep>(io_service_);
    AsyncTaskId task_id = _AddTask(sleep_task);
    handle.AddTask(task_id);
    bool result = sleep_task->Sleep(duration, context);
    handle.RemoveTask(task_id);
    return result;
  }

  virtual bool WaitOnEvent(const AsyncTaskId& event_id, YieldContext& context) override {
    std::shared_ptr<AsyncEvent> event = _GetEvent(event_id);
    if (event) {
      //std::cout << "(Scheduler): Waiting on event " << event_id << std::endl;
      return event->Wait(context);
    } else {
      //std::cout << "(Scheduler): Couldn't find  event " << event_id << std::endl;
      return false;
    }
  }

  virtual AsyncTaskId AddEvent(CoroHandle& handle) override {
    std::shared_ptr<AsyncEvent> event = std::make_shared<AsyncEvent>(io_service_);
    AsyncTaskId task_id = _AddTask(event);
    tasks_[task_id] = std::move(event);
    handle.AddTask(task_id);
    return task_id;
  }

  virtual void FireEvent(const AsyncTaskId& event_id) override {
    std::shared_ptr<AsyncEvent> event = _GetEvent(event_id);
    if (event) {
      io_service_.post([e = std::move(event)]() {
        e->Notify();
      });
    }
  }

  virtual void CancelTask(const AsyncTaskId& task_id) override {
    //std::cout << "Cancelling task " << task_id << std::endl;
    std::shared_ptr<AsyncTask> task = _GetTask(task_id);
    if (task) {
      io_service_.post([t = std::move(task)]() {
        t->Cancel();
      });
    }
  }

//protected:
  AsyncTaskId _AddTask(const std::shared_ptr<AsyncTask>& task) {
    boost::lock_guard<boost::mutex> guard(tasks_mutex_);
    AsyncTaskId task_id = next_task_id_++;
    if (tasks_.find(task_id) != tasks_.end()) {
      return InvalidTaskId;
    }
    tasks_[task_id] = std::move(task);
    return task_id;
  }
  std::shared_ptr<AsyncTask> _GetTask(const AsyncTaskId& task_id) {
    boost::lock_guard<boost::mutex> guard(tasks_mutex_);
    if (tasks_.find(task_id) != tasks_.end()) {
      return tasks_[task_id];
    }
    return nullptr;
  }

  std::shared_ptr<AsyncEvent> _GetEvent(const AsyncTaskId& task_id) {
    std::shared_ptr<AsyncTask> task = _GetTask(task_id);
    if (task) {
      return std::dynamic_pointer_cast<AsyncEvent>(task);
    }
    return nullptr;
  }

  boost::asio::io_service io_service_;
  boost::asio::io_service::work* work_;
  //TODO: to support >1 thread, we need to use strands to make sure the waiting on
  // an event and the notification/cancellation of that event got serialized (or have
  // the event support getting notified/cancelled before it's waited on)
  boost::thread thread_;

  boost::mutex tasks_mutex_;
  AsyncTaskId next_task_id_ = 0; // guarded by tasks_mutex_
  std::map<AsyncTaskId, std::shared_ptr<AsyncTask>> tasks_; // guarded by tasks_mutex_
};
