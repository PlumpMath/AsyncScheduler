WIP

Playing around with the idea of an asynchronous task scheduler that provides some common utilities (semaphores, futures) that can be waited on aysnchronously without blocking the thread (via coroutines).

Just something I tinker around with in free time.  There are some examples of usage in the examples folder.

This was an old branch where I gave out handles to futures and semaphores to the caller so that I could ensure safe access to them.  I abandoned this in favor of giving out the future/semaphore objects directly and having them post access to the single scheduler thread (which is what's in master now).
