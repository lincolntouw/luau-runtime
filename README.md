# luau-runtime    

A minimal standalone Luau runtime with a `task` library (w/ spawn, defer, delay, wait)
and a `fs` library (w/ readFile, writeFile, exists), built on Luau's VM

## Build

```bash
cmake -S . -B build
cmake --build build
```
CMake will fetch the Luau source from GitHub automatically via `FetchContent`. 

## What's here

- `src/runtime.h` / `src/task_lib.cpp` — the scheduler; a queue for current tick work and a heap of sleeping coroutines for `task.wait` / `task.delay`.  
  any other native library (`net`, `process`, `stdio`, ...).
- `src/main.cpp` — boots a `lua_State`, compiles and runs a `.luau` file, then calls `Scheduler::run()` to drain anything spawned/deferred/waiting.
- `src/fs_lib.cpp` — three functions, a small extra library for working with file system 