#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>

#include <functional>
#include <future>
#include <type_traits>

#include "async_future.hpp"
#include "async_sleep.hpp"
#include "async_semaphore.hpp"
#include "scheduler.hpp"
#include "scheduler_types.h"

// Really hacky method of enabled/disabling debug prints for now
static const bool debug = false;
#define DEBUG if (debug) 

// A SchedulerContext represnts a set of coroutines and asynchronous tasks
//  that can be stopped as a group.  This would typically be done when trying to
//  shut down a class which has coroutines and suspensions on these asynchronous tasks,
//  so when a user once to cleanup that object, they'd make sure to call 'Stop' on its
//  SchedulerContext first.  Since a context will only clean up its own coroutines
//  and tasks, other contexts (which share the same master scheduler) will be unaffected.
// In order to prevent synchronizing access to the container which holds those tasks,
//  all access to them is posted through the scheduler's internal thread.
//  This means that, in addition to servicing the tasks the user schedules,
//  the scheduler's thread also has to service some management operations
//  of the scheduler itself.  This allows calls like 'Sleep' to operate
//  efficiently: no (blocking) synchronization is necessary to add and remove the 
//  sleep task to the task container.
class SchedulerContext {
public:
  SchedulerContext(Scheduler& master_scheduler) :
    master_scheduler_(master_scheduler) {
  }

  // this method can be called from any thread (except thread_)
  ~SchedulerContext() {
    //TODO: call stop if not stopped already (?)
  }

  // this method could be called from any thread (except the scheduler thread)
  // it will block until the all the following conditions are met:
  //  1) the running state of the scheduler has been halted, so we know
  //      no more coroutines or asynchronous tasks will be initiated
  //  2) all asynchronous tasks have been cancelled
  //  3) the coroutine has successfully be exited
  // once all of the above have been guaranteed, we know we've shut things
  //  down safely.
  void Stop() {
    assert(master_scheduler_.thread_.get_id() != boost::this_thread::get_id());
    DEBUG printf("SchedulerContext::Stop\n");
    // we need to prevent any new coroutines/async operations
    //  from being started
    auto running_state_updated = std::make_shared<std::promise<bool>>();
    auto running_state_updated_future = running_state_updated->get_future();
    // Post the setting of 'running_' to the scheduler thread so that 
    //  we don't have to lock access to it
    master_scheduler_.io_service_.post([running_state_updated, this]() mutable {
      DEBUG printf("SchedulerContext::Stop inside running state update function, setting to false\n");
      running_ = false;
      running_state_updated->set_value(true);
    });
    DEBUG printf("SchedulerContext::Stop waiting for running state to be updated\n");
    running_state_updated_future.wait();
    DEBUG printf("SchedulerContext::Stop running state updated\n");
    // At this point, we know no new async operations or coroutines will be started
    // we need to cancel any pending async operations (which must
    //  be done via the scheduler thread)
    auto tasks_cancelled = std::make_shared<std::promise<bool>>();
    auto tasks_cancelled_future = tasks_cancelled->get_future();
    master_scheduler_.io_service_.post([tasks_cancelled, this]() mutable {
      DEBUG printf("SchedulerContext::Stop inside task cancel function\n");
      for (auto&& t : tasks_) {
        DEBUG printf("cancelling task %d\n", t.first);
        t.second->Cancel();
      }
      tasks_cancelled->set_value(true);
    });
    DEBUG printf("SchedulerContext::Stop waiting for task cancel\n");
    tasks_cancelled_future.wait();
    DEBUG printf("SchedulerContext::Stop tasks cancelled\n");

    // we need to wait for the coroutines to finish
    // NOTE: we can safely access coro_finished_futures_ from any thread here because
    //  the only other place it gets accessed is from within the scheduler thread
    //  when spawning a new coroutine, and we know that won't happen since that
    //  modification is done inside a check of 'running_' and we've already successfully
    //  set the state of 'running_' to false (above)
    DEBUG printf("SchedulerContext::Stop waiting for coroutine to finish\n");
    for (auto&& cff : coro_finished_futures_) {
      if (cff.valid()) {
        cff.wait();
      }
    }
    DEBUG printf("SchedulerContext::Stop coroutine finished\n");
  }

  // Spawn a new coroutine
  // this method could be called from any thread
  void SpawnCoroutine(std::function<void(boost::asio::yield_context)>&& coro_func) {
    DEBUG printf("SchedulerContext::SpawnCoroutine\n");
    auto coro_completion = std::make_shared<std::promise<bool>>();

    // Wrap the coroutine call so we can automatically set the coroutine completion
    //  future when it exits
    auto coro_wrapper = [coro_completion, cf = std::move(coro_func)](boost::asio::yield_context context) {
      DEBUG printf("Inside coroutine wrapper, running coroutine function\n");
      cf(context);
      DEBUG printf("Inside coroutine wrapper, finished running coroutine function, completing promise\n");
      coro_completion->set_value(true);
    };

    // Wrap the call to spawn so we can check 'running_' from the scheduler thread
    auto spawn_wrapper = [coro_completion, c = std::move(coro_wrapper), this]() {
      assert(master_scheduler_.thread_.get_id() == boost::this_thread::get_id());
      DEBUG printf("Inside spawn wrapper, checking running state\n");
      if (running_) {
        coro_finished_futures_.push_back(coro_completion->get_future());
        DEBUG printf("Inside spawn wrapper, still running, spawning coroutine \n");
        boost::asio::spawn(master_scheduler_.io_service_, std::move(c));
      } else {
        DEBUG printf("Inside spawn wrapper, not running, not spawning coroutine\n");
        coro_completion->set_value(false);
      }
    };

    //TODO: this should be a dispatch. change it and add tests for both (called from thread_ and another thread)
    // cases
    master_scheduler_.io_service_.post(std::move(spawn_wrapper));
  }

  // Asynchronously sleep for the given duration.  Returns true if the 
  //  sleep executed successfully (slept for the entire time), false if
  //  it was awoken early (the sleep task was cancelled)
  // Can only be called from this scheduler's worker thread
  // (i.e. this should only be called from within a coroutine
  // spawned by this scheduler)
  bool Sleep(
      const std::chrono::duration<double>& duration,
      boost::asio::yield_context& context) {
    assert(master_scheduler_.thread_.get_id() == boost::this_thread::get_id());
    if (running_) {
      auto sleep = std::make_shared<AsyncSleep>(master_scheduler_.io_service_);
      auto task_id = next_task_id_++;
      tasks_[task_id] = sleep;
      auto res = sleep->Sleep(duration, context);
      tasks_.erase(task_id);
      return res;
    }
    return false;
  }

  // Post a provided function to the scheduler to be executed by the scheduler's thread.
  // Specialization for void funcs
  // NOTE: we don't need to track the completion of the posted functions added of
  //  the 'Post' methods because, since they're executed synchronously, we know they'll
  //  be done by the time 'Stop' would have gotten around to waiting on their futures
  //  (since that code would occur on the same thread that the executed functions execute
  //  on)
  template<typename Functor, typename = typename std::enable_if<std::is_void<typename std::result_of<Functor()>::type>::value>::type>
  void
  Post(const Functor& func) {
    auto func_wrapper = [func = std::move(func), this]() mutable {
      if (running_) {
        func();
      }
    };
    master_scheduler_.io_service_.post(std::move(func_wrapper));
  }

  // Post for non-void funcs which returns a handle to an AsyncFuture
  // Call with something like: scheduler.Post(func, SchedulerContext::UseAsync);
  template<typename Functor, typename = typename std::enable_if<!std::is_void<typename std::result_of<Functor()>::type>::value>::type>
  task_handle_t
  Post(const Functor& func, async_type_t) {
    auto future_handle = CreateFuture<typename std::result_of<Functor()>::type>();
    auto func_wrapper = [func = std::move(func), future_handle, this]() mutable {
      if (running_) {
        SetFutureValue(future_handle, func());
      }
    };
    master_scheduler_.io_service_.post(std::move(func_wrapper));
    return future_handle;
  }

  // For non-void funcs.  Returns a blocking future
  template<typename Functor, typename = typename std::enable_if<!std::is_void<typename std::result_of<Functor()>::type>::value>::type>
  std::future<typename std::result_of<Functor()>::type>
  Post(const Functor& func) {
    auto func_promise = std::make_shared<std::promise<typename std::result_of<Functor()>::type>>();
    auto func_future = func_promise->get_future();
    auto func_wrapper = [func = std::move(func), func_promise, this]() mutable {
      if (running_) {
        func_promise->set_value(func());
      }
    };
    master_scheduler_.io_service_.post(std::move(func_wrapper));
    return func_future;
  }

  // Returns a handle to the created semaphore
  // could be called from any thread
  task_handle_t CreateSemaphore() {
    auto promise = std::make_shared<std::promise<int>>();
    auto future = promise->get_future();
    master_scheduler_.io_service_.dispatch([promise, this]() mutable {
      auto semaphore = std::make_shared<AsyncSemaphore>(master_scheduler_.io_service_);
      auto task_id = next_task_id_++;
      tasks_[task_id] = semaphore;
      promise->set_value(task_id);
    });
    return future.get();
  }

  // could be called from any thread
  void RaiseSemaphore(int semaphore_handle) {
    master_scheduler_.io_service_.dispatch([semaphore_handle, this]() {
      if (tasks_.find(semaphore_handle) != tasks_.end()) {
        auto semaphore = std::dynamic_pointer_cast<AsyncSemaphore>(tasks_[semaphore_handle]);
        if (semaphore) {
          semaphore->Raise();
        }
      }
    });
  }

  // Must be called from this scheduler's thread
  // (i.e. inside a coroutine spawned by this scheduler)
  bool WaitOnSemaphore(int semaphore_handle, boost::asio::yield_context& context) {
    assert(master_scheduler_.thread_.get_id() == boost::this_thread::get_id());
    if (tasks_.find(semaphore_handle) != tasks_.end()) {
      auto semaphore = std::dynamic_pointer_cast<AsyncSemaphore>(tasks_[semaphore_handle]);
      if (semaphore) {
        return semaphore->Wait(context);
      }
    }
    return false;
  }

  // We can afford to block on getting the future here because:
  // 1) If this is called on the scheduler thread, 'dispatch'
  //     will execute the posted code inline, so we'll get it immediately
  // 2) If this is called on a non-scheduler thread, then we know the scheduler
  //     thread is free to service the posted task and will complete the future
  //     soon.  Note: because this may block, it shouldn't be called in a place
  //     where that's unacceptable.
  template<typename T>
  task_handle_t CreateFuture() {
    auto promise = std::make_shared<std::promise<int>>();
    auto future = promise->get_future();
    master_scheduler_.io_service_.dispatch([promise, this]() mutable {
      auto semaphore = std::make_shared<AsyncFuture<T>>(master_scheduler_.io_service_);
      auto task_id = next_task_id_++;
      tasks_[task_id] = semaphore;
      promise->set_value(task_id);
    });
    return future.get();
  }
  
  // could be called from any thread
  template<typename T>
  void SetFutureValue(int future_handle, T value) {
    //TODO: change to dispatch
    master_scheduler_.io_service_.post([future_handle, value, this]() {
      if (tasks_.find(future_handle) != tasks_.end()) {
        auto future = std::dynamic_pointer_cast<AsyncFuture<T>>(tasks_[future_handle]);
        if (future) {
          future->SetValue(value);
        }
      }
    });
  }

  // Can only be called from the scheduler's thread
  // (i.e. this should only be called from within a coroutine
  // spawned by this scheduler)
  //TODO: if we make the handle contain the type as well, we can relieve
  // the caller from having to explicitly write the type when calling 
  // WaitOnFuture (right now it has to be 'auto res = WaitOnFuture<bool>(...)', 
  // for example
  template<typename T>
  boost::optional<T> WaitOnFuture(int future_handle, boost::asio::yield_context context) {
    assert(master_scheduler_.thread_.get_id() == boost::this_thread::get_id());
    if (tasks_.find(future_handle) != tasks_.end()) {
      auto future = std::dynamic_pointer_cast<AsyncFuture<T>>(tasks_[future_handle]);
      if (future) {
        return future->Get(context);
      }
    }
    return false;
  }

//protected:
  Scheduler& master_scheduler_;

  // Members below here can only be accesed via thread_
  bool running_ = true;
  int next_task_id_ = 0;

  std::map<int, std::shared_ptr<AsyncTask>> tasks_;
  std::list<std::future<bool>> post_func_finished_futures_;
  std::list<std::future<bool>> coro_finished_futures_;
};
