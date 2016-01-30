#pragma once

#include <future>

typedef int AsyncTaskId;
static AsyncTaskId InvalidTaskId = -1;

typedef std::future<bool> coro_completion_t;
