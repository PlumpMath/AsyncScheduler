#pragma once

class AsyncTask {
public:
  virtual ~AsyncTask() {}
  virtual void Cancel() = 0;
};
