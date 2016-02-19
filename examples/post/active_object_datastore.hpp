#pragma once

#include <boost/optional.hpp>
#include <list>

#include "scheduler.hpp"

// Rather than synchronizing access to data_ via locks, it can instead be done
//  by posting all operations on it to a single thread.
template<typename Key, typename Value>
class AODataStore {
public:
  void AddValue(Key key, Value value) {
    return scheduler_.Post(std::bind(&AODataStore::_DoAddValue, this, key, value));
  }

  boost::optional<Value> GetValue(int key) {
    auto res = scheduler_.Post(std::bind(&AODataStore::_DoGetValue, this, key));
    // We could just return the future here, but doing it this way totally hides
    //  the asynchronous behavior of the AODataStore implementation from the user
    return res.get();
  }

  void RemoveValue(int key) {
    return scheduler_.Post(std::bind(&AODataStore::_DoRemoveValue, this, key));
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

  Scheduler scheduler_;
  std::map<Key, Value> data_;
};
