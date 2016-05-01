#pragma once

#include <boost/optional.hpp>
#include <list>

#include "scheduler_context.hpp"

// Rather than synchronizing access to data_ via locks, it can instead be done
//  by posting all operations on it to a single thread.
template<typename Key, typename Value>
class DataStore {
public:
  DataStore(Scheduler& master_scheduler) : scheduler_(master_scheduler) {}
  void AddValue(Key key, Value value) {
    return scheduler_.Post(std::bind(&DataStore::_DoAddValue, this, key, value));
  }

  boost::optional<Value> GetValue(int key) {
    auto res = scheduler_.Post(std::bind(&DataStore::_DoGetValue, this, key));
    // We could just return the future here, but doing it this way totally hides
    //  the asynchronous behavior of the DataStore implementation from the user
    return res.get();
  }

  void RemoveValue(int key) {
    return scheduler_.Post(std::bind(&DataStore::_DoRemoveValue, this, key));
  }

//protected:
  void _DoAddValue(Key key, Value value) {
    data_[key] = value;
  }

  boost::optional<Value> _DoGetValue(int key) {
    if (data_.find(key) != data_.end()) {
      return data_[key];
    }
    return boost::none;
  }

  void _DoRemoveValue(int key) {
    data_.erase(key);
  }

  SchedulerContext scheduler_;
  std::map<Key, Value> data_;
};
