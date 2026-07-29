#include "runtime.h"
#include "lualib.h"
#include <iostream>
#include <thread>

Scheduler& Scheduler::instance() {
    static Scheduler s;
    return s;
}

double Scheduler::now() const {
    using namespace std::chrono;
    return duration<double>(steady_clock::now() - startTime).count();
}

void Scheduler::spawnNow(lua_State* L, lua_State* thread, int threadRef, int nargs) {
    readyQueue.push_back({thread, threadRef, nargs});
}

void Scheduler::deferNow(lua_State* L, lua_State* thread, int threadRef, int nargs) {
    readyQueue.push_back({thread, threadRef, nargs});
}

void Scheduler::scheduleAfter(lua_State* L, double seconds, lua_State* thread, int threadRef, int nargs) {
    delayHeap.push({now() + seconds, thread, threadRef, nargs});
}

void Scheduler::resumeThread(lua_State* L, lua_State* thread, int threadRef, int nargs) {
    runningThread = thread;
    runningThreadRef = threadRef;

    int status = lua_resume(thread, L, nargs);

    runningThread = nullptr;
    runningThreadRef = -1;

    if (status != LUA_YIELD && status != 0) {
        const char* msg = lua_tostring(thread, -1);
        std::cerr << "task error: " << (msg ? msg : "unknown error") << "\n";
    }

    if (status != LUA_YIELD) {
        // finished or errored, release keepalive ref 
        lua_unref(L, threadRef);
    }
}

void Scheduler::run(lua_State* L) {
    while (!readyQueue.empty() || !delayHeap.empty()) {
        // drain anything ready right now
        std::vector<std::tuple<lua_State*, int, int>> batch;
        std::swap(batch, readyQueue);
        for (auto& [thread, ref, nargs] : batch) { resumeThread(L, thread, ref, nargs); }

        // wake anything whose time has come 
        double t = now();
        while (!delayHeap.empty() && delayHeap.top().wakeTime <= t) {
            DelayedThread d = delayHeap.top();
            delayHeap.pop();
            resumeThread(L, d.thread, d.threadRef, d.nargs);
        }

        // if nothing is ready but something is delayed, sleep until it fires
        if (readyQueue.empty() && !delayHeap.empty()) {
            double sleepFor = delayHeap.top().wakeTime - now();
            if (sleepFor > 0) std::this_thread::sleep_for(std::chrono::duration<double>(sleepFor));
        }
    }
}

bool Scheduler::cancel(lua_State* L, lua_State* target) {       
    auto it = std::find_if(readyQueue.begin(), readyQueue.end(),
        [target](const std::tuple<lua_State*, int, int>& entry) {
            return std::get<0>(entry) == target;
        });

    if (it != readyQueue.end()) {
        int ref = std::get<1>(*it);   
        lua_unref(L, ref);   
        readyQueue.erase(it);      
        return true;    
    }
            
    std::vector<DelayedThread> remaining;
    bool found = false;

    while (!delayHeap.empty()) {
        DelayedThread d = delayHeap.top();  
        delayHeap.pop();    

        if (!found && d.thread == target) {
            lua_unref(L, d.threadRef);
            found = true;
            continue;                   
        }
        remaining.push_back(d);
    }
    for (const DelayedThread& d : remaining) { delayHeap.push(d); }
    return found;
}

// -- HELPERS -- //

// Pulls a function & its args off L's stack (indices 1..N), moves them onto a brand-new coroutine, and returns that coroutine ready to be resumed.
static lua_State* makeThreadFromArgs(lua_State* L, int& outRef, int& outNargs) {
    int nargs = lua_gettop(L) - 1; // args after the function; captured before pushing the thread
    lua_State* thread = lua_newthread(L); // L stack is now: [func, arg1..argN, thread]

    lua_insert(L, 1); // move thread to bottom: [thread, func, arg1..argN]
    lua_xmove(L, thread, nargs + 1); // move func&args (top nargs+1 values) onto the new thread

    // L's stack is now just [thread]  
    outRef = lua_ref(L, -1);
    outNargs = nargs;
    return thread;
}

// -- task.* -- //          

static int lua_task_spawn(lua_State* L) {
    int ref, nargs;
    lua_State* thread = makeThreadFromArgs(L, ref, nargs);
    // spawn runs immediately, synchronously, exactly once; is not queued again
    Scheduler::instance().resumeThread(L, thread, ref, nargs);
    return 1;
}

static int lua_task_defer(lua_State* L) {
    int ref, nargs;
    lua_State* thread = makeThreadFromArgs(L, ref, nargs);
    Scheduler::instance().deferNow(L, thread, ref, nargs);
    return 1;
}

static int lua_task_delay(lua_State* L) {
    double seconds = luaL_checknumber(L, 1);
    lua_remove(L, 1); // shift so makeThreadFromArgs sees the function at index 1
    int ref, nargs;
    lua_State* thread = makeThreadFromArgs(L, ref, nargs);
    Scheduler::instance().scheduleAfter(L, seconds, thread, ref, nargs);
    return 1;
}

static int lua_task_cancel(lua_State* L) {   
    luaL_checktype(L, 1, LUA_TTHREAD);
    lua_State* target = lua_tothread(L, 1);
    Scheduler::instance().cancel(L, target);
    return 0;
}

static int lua_task_wait(lua_State* L) {
    double seconds = luaL_optnumber(L, 1, 0.0);
    Scheduler& s = Scheduler::instance();

    // L here is the currently running coroutine; the scheduler already knows its thread&ref because it set them right before resuming; reuse those & don't invent a new ref
    lua_State* self = s.runningThread;
    int selfRef = s.runningThreadRef;

    s.scheduleAfter(L, seconds, self, selfRef, 0);
    return lua_yield(L, 0);
}

void registerTaskLibrary(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_task_spawn, "spawn");
    lua_setfield(L, -2, "spawn");

    lua_pushcfunction(L, lua_task_defer, "defer");
    lua_setfield(L, -2, "defer");
     
    lua_pushcfunction(L, lua_task_delay, "delay");
    lua_setfield(L, -2, "delay");

    lua_pushcfunction(L, lua_task_cancel, "cancel");
    lua_setfield(L, -2, "cancel");
       
    lua_pushcfunction(L, lua_task_wait, "wait");
    lua_setfield(L, -2, "wait");

    lua_setglobal(L, "task");
}
   
