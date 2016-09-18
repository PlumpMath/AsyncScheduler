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
#define DEBUG_LOG if (debug) 

// A SchedulerContext represents a set of coroutines and asynchronous tasks
//  that can be stopped as a group.  This would typically be done when trying to
//  shut down a class which has coroutines and suspensions on these asynchronous tasks,
//  so when a user wants to cleanup that object, they'd make sure to call 'Stop' on its
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

  // this method can be called from any thread except the scheduler's internal thread
  ~SchedulerContext() {
    assert(master_scheduler_.thread_.get_id() != boost::this_thread::get_id());
    //TODO: call stop if not stopped already (?)
  }

  boost::asio::io_service& GetIoService() {
    return master_scheduler_.io_service_;
  }

  void VerifyInIoServiceThread() {
    assert(master_scheduler_.thread_.get_id() == boost::this_thread::get_id());
  }

  // Can be called from anythread except the scheduler's internal thread
  // it will block until the all the following conditions are met:
  //  1) the running state of the scheduler has been halted, so we know
  //      no more coroutines or asynchronous tasks will be initiated
  //  2) all asynchronous tasks have been cancelled
  //  3) the coroutine has successfully be exited
  // once all of the above have been guaranteed, we know we've shut things
  //  down safely.
  void Stop() {
    assert(master_scheduler_.thread_.get_id() != boost::this_thread::get_id());
    DEBUG_LOG printf("SchedulerContext::Stop\n");
    // Helper to post a function to the scheduler thread and wait until
    //  its execution is finished
    auto postAndWaitHelper = [this](auto func) {
      auto executed = std::make_shared<std::promise<bool>>();
      auto executed_future = executed->get_future();
      master_scheduler_.io_service_.post([executed, &func]() {
        func();
        executed->set_value(true);
      });
      executed_future.wait();
    };
    // we need to prevent any new coroutines/async operations
    //  from being started
    // Post the setting of 'running_' to the scheduler thread so that 
    //  we don't have to lock access to it
    DEBUG_LOG printf("SchedulerContext::Stop waiting for running state to be updated\n");
    postAndWaitHelper([this]() {
      running_ = false;
    });
    DEBUG_LOG printf("SchedulerContext::Stop running state updated\n");
    // At this point, we know no new async operations or coroutines will be started

    // we need to cancel any pending async operations (which must
    //  be done via the scheduler thread)
    DEBUG_LOG printf("SchedulerContext::Stop waiting for task cancel\n");
    postAndWaitHelper([this]() {
      for (auto&& t : tasks_) {
        DEBUG_LOG printf("cancelling task %d\n", t.first);
        t.second->Cancel();
      }
    });
    DEBUG_LOG printf("SchedulerContext::Stop tasks cancelled\n");
    // wait for all coroutines to finish
    // NOTE: we can safely access coro_finished_futures_ from any thread here because
    //  the only other place it gets accessed is from within the scheduler thread
    //  when spawning a new coroutine, and we know that won't happen since that
    //  modification is done inside a check of 'running_' and we've already successfully
    //  set the state of 'running_' to false (above)
    DEBUG_LOG printf("SchedulerContext::Stop waiting for coroutine to finish\n");
    for (auto&& cff : coro_finished_futures_) {
      if (cff.valid()) {
        cff.wait();
      }
    }
    DEBUG_LOG printf("SchedulerContext::Stop coroutine finished\n");
  }

  // Spawn a new coroutine
  // this method could be called from any thread
  void SpawnCoroutine(std::function<void(boost::asio::yield_context)>&& coro_func) {
    DEBUG_LOG printf("SchedulerContext::SpawnCoroutine\n");
    auto coro_completion = std::make_shared<std::promise<bool>>();

    // Wrap the coroutine call so we can automatically set the coroutine completion
    //  future when it exits
    auto coro_wrapper = [coro_completion, cf = std::move(coro_func)](boost::asio::yield_context context) {
      DEBUG_LOG printf("Inside coroutine wrapper, running coroutine function\n");
      cf(context);
      DEBUG_LOG printf("Inside coroutine wrapper, finished running coroutine function, completing promise\n");
      coro_completion->set_value(true);
    };

    // Wrap the call to spawn so we can check 'running_' from the scheduler thread
    auto spawn_wrapper = [coro_completion, c = std::move(coro_wrapper), this]() {
      assert(master_scheduler_.thread_.get_id() == boost::this_thread::get_id());
      DEBUG_LOG printf("Inside spawn wrapper, checking running state\n");
      if (running_) {
        coro_finished_futures_.push_back(coro_completion->get_future());
        DEBUG_LOG printf("Inside spawn wrapper, still running, spawning coroutine \n");
        boost::asio::spawn(master_scheduler_.io_service_, std::move(c));
      } else {
        DEBUG_LOG printf("Inside spawn wrapper, not running, not spawning coroutine\n");
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
  //  (i.e. this should only be called from within a coroutine
  //  spawned by this scheduler)
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
  // Can be called from any thread
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
    // we don't 'dispatch' here so we don't sometimes execute inline and sometimes async
    master_scheduler_.io_service_.post(std::move(func_wrapper));
  }

  // Post for non-void funcs which returns an AsyncFuture
  // Call with something like: scheduler.Post(func, SchedulerContext::UseAsync);
  // Can be called from any thread
  template<typename Functor, typename = typename std::enable_if<!std::is_void<typename std::result_of<Functor()>::type>::value>::type>
  std::shared_ptr<AsyncFuture<typename std::result_of<Functor()>::type>>
  Post(const Functor& func, async_type_t) {
    auto future = CreateFuture<typename std::result_of<Functor()>::type>();
    auto func_wrapper = [func = std::move(func), future, this]() mutable {
      if (running_) {
        future->SetValue(func());
      }
      //TODO: what value do we set on the promise if we're no longer running?
    };
    master_scheduler_.io_service_.post(std::move(func_wrapper));
    return future;
  }

  // For non-void funcs.  Returns a blocking future
  // Can be called from any thread
  template<typename Functor, typename = typename std::enable_if<!std::is_void<typename std::result_of<Functor()>::type>::value>::type>
  std::future<typename std::result_of<Functor()>::type>
  Post(const Functor& func) {
    auto func_promise = std::make_shared<std::promise<typename std::result_of<Functor()>::type>>();
    auto func_future = func_promise->get_future();
    auto func_wrapper = [func = std::move(func), func_promise, this]() mutable {
      if (running_) {
        func_promise->set_value(func());
      }
      //TODO: what value do we set on the promise if we're no longer running?
    };
    master_scheduler_.io_service_.post(std::move(func_wrapper));
    return func_future;
  }

  // Can be called from any thread
  std::shared_ptr<AsyncSemaphore> CreateSemaphore() {
    auto promise = std::make_shared<std::promise<std::shared_ptr<AsyncSemaphore>>>();
    auto future = promise->get_future();
    master_scheduler_.io_service_.dispatch([promise, this]() mutable {
      auto semaphore = std::make_shared<AsyncSemaphore>(master_scheduler_.io_service_);
      auto task_id = next_task_id_++;
      tasks_[task_id] = semaphore;
      promise->set_value(semaphore);
    });
    return future.get();
  }

  // Can be called from any thread
  template<typename T>
  std::shared_ptr<AsyncFuture<T>> CreateFuture() {
    auto promise = std::make_shared<std::promise<std::shared_ptr<AsyncFuture<T>>>>();
    auto future = promise->get_future();
    master_scheduler_.io_service_.dispatch([promise, this]() mutable {
      auto future = std::make_shared<AsyncFuture<T>>(master_scheduler_.io_service_);
      auto task_id = next_task_id_++;
      tasks_[task_id] = future;
      promise->set_value(future);
    });
    return future.get();
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
