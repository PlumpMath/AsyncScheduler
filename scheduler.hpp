#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include <functional>
#include <future>
#include <type_traits>

#include "async_future.hpp"
#include "async_sleep.hpp"
#include "async_semaphore.hpp"

#include <boost/bind.hpp>

// Single-threaded coroutine scheduler.
// The scheduler will keep track of all coroutines as well as
//  all asynchronous tasks so that it can be shut down cleanly.
//  in order to do so, it must keep track of all the asynchronous task
//  objects so that it can cancel them when the user wants.  In order to
//  prevent synchronizing access to the container which holds those tasks,
//  all access to them is posted through the scheduler's internal thread.
//  This means that, in addition to servicing the tasks the user schedules,
//  the scheduler's thread also has to service some management operations
//  of the scheduler itself.  This allows calls like 'Sleep' to operate
//  efficiently: no (blocking) synchronization is necessary to add and remove the 
//  sleep task to the task container.
class Scheduler {
public:
  typedef int task_handle_t;
  Scheduler() :
    work_(new boost::asio::io_service::work(io_service_)),
    thread_(boost::bind(&boost::asio::io_service::run, &io_service_)) {
  }

  // this method can be called from any thread (except thread_)
  ~Scheduler() {
    assert(thread_.get_id() != boost::this_thread::get_id());
    io_service_.stop();
    thread_.join();
  }

  // this method could be called from any thread (except the scheduler thread)
  // it will block until the all the following conditions are met:
  //  1) the running state of the scheduler has been halted, so we know
  //      no more coroutines or asynchronous tasks will be initiated
  //  2) all asynchronous tasks have been cancelled
  //  3) all 'posted' functions have finished
  //  4) the coroutine has successfully be exited
  // once all of the above have been guaranteed, we know we've shut things
  //  down safely.
  void Stop() {
    assert(thread_.get_id() != boost::this_thread::get_id());
    printf("Scheduler::Stop\n");
    // we need to prevent any new coroutines/async operations
    //  from being started
    auto running_state_updated = std::make_shared<std::promise<bool>>();
    auto running_state_updated_future = running_state_updated->get_future();
    // Post the setting of 'running_' to the scheduler thread so that 
    //  we don't have to lock access to it
    io_service_.post([running_state_updated, this]() mutable {
      printf("Scheduler::Stop inside running state update function, setting to false\n");
      running_ = false;
      running_state_updated->set_value(true);
    });
    printf("Scheduler::Stop waiting for running state to be updated\n");
    running_state_updated_future.wait();
    printf("Scheduler::Stop running state updated\n");
    // At this point, we know no new async operations or coroutines will be started
    // we need to cancel any pending async operations (which must
    //  be done via the scheduler thread)
    auto tasks_cancelled = std::make_shared<std::promise<bool>>();
    auto tasks_cancelled_future = tasks_cancelled->get_future();
    io_service_.post([tasks_cancelled, this]() mutable {
      printf("Scheduler::Stop inside task cancel function\n");
      for (auto&& t : tasks_) {
        t.second->Cancel();
      }
      tasks_cancelled->set_value(true);
    });
    printf("Scheduler::Stop waiting for task cancel\n");
    tasks_cancelled_future.wait();
    printf("Scheduler::Stop tasks cancelled\n");
    // we need to make sure all posted functions have finished
    auto functions_finished = std::make_shared<std::promise<bool>>();
    auto functions_finisheds_future = functions_finished->get_future();
    io_service_.post([functions_finished, this]() mutable {
      printf("Scheduler::Stop inside waiting for posted functions to finish\n");
      for (auto&& f : post_func_finished_futures_) {
        f.wait();
      }
      functions_finished->set_value(true);
    });
    printf("Scheduler::Stop waiting for all posted functions to finished\n");
    functions_finisheds_future.wait();
    printf("Scheduler::Stop all posted functions finished\n");

    // we need to wait for the coroutines to finish
    printf("Scheduler::Stop waiting for coroutine to finish\n");
    for (auto&& cff : coro_finished_futures_) {
      if (cff.valid()) {
        cff.wait();
      }
    }
    printf("Scheduler::Stop coroutine finished\n");
  }

  // this method could be called from any thread
  void SpawnCoroutine(std::function<void(boost::asio::yield_context)>&& coro_func) {
    printf("Scheduler::SpawnCoroutine\n");
    auto coro_completion = std::make_shared<std::promise<bool>>();
    coro_finished_futures_.push_back(coro_completion->get_future());

    // Wrap the coroutine call so we can automatically set the coroutine completion
    //  future when it exits
    auto coro_wrapper = [coro_completion, cf = std::move(coro_func)](boost::asio::yield_context context) {
      printf("Inside coroutine wrapper, running coroutine function\n");
      cf(context);
      printf("Inside coroutine wrapper, finished running coroutine function, completing promise\n");
      coro_completion->set_value(true);
    };

    // Wrap the call to spawn so we can check 'running_' from the scheduler thread
    auto spawn_wrapper = [coro_completion, c = std::move(coro_wrapper), this]() {
      assert(thread_.get_id() == boost::this_thread::get_id());
      printf("Inside spawn wrapper, checking running state\n");
      if (running_) {
        printf("Inside spawn wrapper, still running, spawning coroutine \n");
        boost::asio::spawn(io_service_, std::move(c));
      } else {
        printf("Inside spawn wrapper, not running, not spawning coroutine\n");
        coro_completion->set_value(false);
      }
    };

    io_service_.post(std::move(spawn_wrapper));
  }

  // Can only be called from this scheduler's worker thread
  // (i.e. this should only be called from within a coroutine
  // spawned by this scheduler)
  bool Sleep(
      const std::chrono::duration<double>& duration,
      boost::asio::yield_context& context) {
    assert(thread_.get_id() == boost::this_thread::get_id());
    if (running_) {
      auto sleep = std::make_shared<AsyncSleep>(io_service_);
      auto task_id = next_task_id++;
      tasks_[task_id] = sleep;
      auto res = sleep->Sleep(duration, context);
      tasks_.erase(task_id);
      return res;
    }
    return false;
  }

  // Specialization for void funcs
  template<typename Functor, typename = typename std::enable_if<std::is_void<typename std::result_of<Functor()>::type>::value>::type>
  void
  Post(const Functor& func) {
    auto func_wrapper = [func, this]() mutable {
      if (running_) {
        std::promise<bool> post_func_ran_p;
        post_func_finished_futures_.push_back(post_func_ran_p.get_future());
        func();
        post_func_ran_p.set_value(true);
      } else {
        //TODO: should we notify the caller in some way if we're not going to
        // run it at all?
      }
    };
    io_service_.post(std::move(func_wrapper));
  }

  // For non-void funcs
  template<typename Functor, typename = typename std::enable_if<!std::is_void<typename std::result_of<Functor()>::type>::value>::type>
  std::future<typename std::result_of<Functor()>::type>
  Post(const Functor& func) {
    auto func_promise = std::make_shared<std::promise<typename std::result_of<Functor()>::type>>();
    auto func_future = func_promise->get_future();
    auto func_wrapper = [func, func_promise, this]() mutable {
      if (running_) {
        std::promise<bool> post_func_ran_p;
        post_func_finished_futures_.push_back(post_func_ran_p.get_future());
        func_promise->set_value(func());
        post_func_ran_p.set_value(true);
      } else {
        //TODO: should we notify the caller in some way if we're not going to
        // run it at all?
      }
    };
    io_service_.post(std::move(func_wrapper));
    return func_future;
  }

  // Returns a future to a handle to the created semaphore
  std::future<int> CreateSemaphore() {
    auto promise = std::make_shared<std::promise<int>>();
    auto future = promise->get_future();
    io_service_.post([promise, this]() mutable {
      auto semaphore = std::make_shared<AsyncSemaphore>(io_service_);
      auto task_id = next_task_id++;
      tasks_[task_id] = semaphore;
      promise->set_value(task_id);
    });
    return future;
  }

  // could be called from any thread
  // TODO: for methods that COULD be called from the scheduler thread, 
  //  we might want to look at using 'dispatch' instead of 'post'
  void RaiseSemaphore(int semaphore_handle) {
    io_service_.post([semaphore_handle, this]() {
      if (tasks_.find(semaphore_handle) != tasks_.end()) {
        auto semaphore = std::dynamic_pointer_cast<AsyncSemaphore>(tasks_[semaphore_handle]);
        if (semaphore) {
          semaphore->Raise();
        }
      } else {
        //TODO
      }
    });
  }

  // Can only be called from this scheduler's worker thread
  // (i.e. this should only be called from within a coroutine
  // spawned by this scheduler)
  bool WaitOnSemaphore(int semaphore_handle, boost::asio::yield_context& context) {
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
    io_service_.dispatch([promise, this]() mutable {
      auto semaphore = std::make_shared<AsyncFuture<T>>(io_service_);
      auto task_id = next_task_id++;
      tasks_[task_id] = semaphore;
      promise->set_value(task_id);
    });
    return future.get();
  }
  
  template<typename T>
  void SetFutureValue(int future_handle, T value) {
    io_service_.post([future_handle, value, this]() {
      if (tasks_.find(future_handle) != tasks_.end()) {
        auto future = std::dynamic_pointer_cast<AsyncFuture<T>>(tasks_[future_handle]);
        if (future) {
          future->SetValue(value);
        }
      } else {
        //TODO
      }
    });
  }


  // Can only be called from the scheduler's thread
  // (i.e. this should only be called from within a coroutine
  // spawned by this scheduler)
  template<typename T>
  boost::optional<T> WaitOnFuture(int future_handle, boost::asio::yield_context context) {
    if (tasks_.find(future_handle) != tasks_.end()) {
      auto future = std::dynamic_pointer_cast<AsyncFuture<T>>(tasks_[future_handle]);
      if (future) {
        return future->Get(context);
      }
    }
    return false;
  }


//protected:
  boost::asio::io_service io_service_;
  boost::asio::io_service::work* work_;

  // Can only be accesed via thread_
  bool running_ = true;

  boost::thread thread_;

  // Can only be accessed via thread_
  int next_task_id = 0;

  // Can only be accessed via thread_
  std::map<int, std::shared_ptr<AsyncTask>> tasks_;
  std::list<std::future<bool>> post_func_finished_futures_;

  std::list<std::future<bool>> coro_finished_futures_;
};
