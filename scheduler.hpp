#pragma once

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/thread.hpp>

#include <functional>
#include <future>

#include "async_sleep.hpp"

#include <boost/bind.hpp>

class Scheduler {
public:
  Scheduler() :
    work_(new boost::asio::io_service::work(io_service_)),
    thread_(boost::bind(&boost::asio::io_service::run, &io_service_)) {
    //thread_(std::bind(&boost::asio::io_service::run, &io_service_)) {
  }

  ~Scheduler() {
    io_service_.stop();
    thread_.join();
  }

  // this method could be called from any thread (except the scheduler thread)
  void Stop() {
    printf("Scheduler::Stop\n");
    // it will block until the all the following conditions are met
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
    // we need to wait for the coroutine to finish
    printf("Scheduler::Stop waiting for coroutine to finish\n");
    if (coro_finished_future_.valid()) {
      coro_finished_future_.wait();
    }
    printf("Scheduler::Stop coroutine finished\n");
  }

  // this method could be called from any thread
  void SpawnCoroutine(std::function<void(boost::asio::yield_context)>&& coro_func) {
    printf("Scheduler::SpawnCoroutine\n");
    auto coro_completion = std::make_shared<std::promise<bool>>();
    coro_finished_future_ = coro_completion->get_future();

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

  // We know the thread executing this method will always be thread_,
  //  since it has to be called from within the coroutine (to get the
  //  context) and the coroutine is always executed by thread_
  bool Sleep(
      const std::chrono::duration<double>& duration,
      boost::asio::yield_context& context) {
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

  std::future<bool> coro_finished_future_;
};
