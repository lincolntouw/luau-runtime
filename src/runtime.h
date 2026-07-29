#pragma once

#include "lua.h"
#include "lualib.h"

#include <vector>
#include <queue>
#include <tuple>
#include <chrono>

// Single suspended coroutine waiting for a wake-up time (such as with task.wait / task.delay).
struct DelayedThread {
    double wakeTime; // seconds; monotonic clock
    lua_State* thread; // coroutine to resume
    int threadRef; // reference into the registry so to not be GC'd early
    int nargs; // args waiting on threads stack for its first resume

    bool operator>(const DelayedThread& other) const {
        return wakeTime > other.wakeTime;
    }      
};
 
// The heart: a ready queue for this-tick work, and a min-heap of everything sleeping until some future time.
class Scheduler {
public:
    static Scheduler& instance();

    void spawnNow(lua_State* L, lua_State* thread, int threadRef, int nargs);
    void deferNow(lua_State* L, lua_State* thread, int threadRef, int nargs);
    void scheduleAfter(lua_State* L, double seconds, lua_State* thread, int threadRef, int nargs = 0);

    // runs until both ready queue and delay heap are empty
    void run(lua_State* L);

    double now() const;
    bool cancel(lua_State* L, lua_State* target);

    /* Resumes one thread and handles its bookkeeping(unref on completion / error).
    Public because task.spawn needs to run its thread synchronously, once,
    right when it's called; not queue it and also run it immediately. */
    void resumeThread(lua_State* L, lua_State* thread, int threadRef, int nargs = 0);

    /* Valid only while a thread is actively being resumed.task.wait uses this
    to find itself so it can reschedule its own thread+ref correctly instead of guessing from the stack. */
    lua_State* runningThread = nullptr;
    int runningThreadRef = -1;

private:
    std::vector<std::tuple<lua_State*, int, int>> readyQueue; // thread, ref, nargs
    std::priority_queue<DelayedThread, std::vector<DelayedThread>, std::greater<>> delayHeap;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

// Registers (task.spawn / task.defer / task.wait / task.delay / task.cancel) into the global "task" table.
void registerTaskLibrary(lua_State* L);

// Registers (fs.readFile / fs.writeFile / fs.exists) into the global "fs" table.
void registerFsLibrary(lua_State* L);
    
