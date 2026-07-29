cmake_minimum_required(VERSION 3.16)
project(luau_runtime CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# pull the official Luau source. pin tag at some point
FetchContent_Declare(
    luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG master
)
FetchContent_MakeAvailable(luau)

add_executable(luau_runtime
    src/main.cpp
    src/task_lib.cpp
    src/fs_lib.cpp
)

target_include_directories(luau_runtime PRIVATE
    ${luau_SOURCE_DIR}/VM/include
    ${luau_SOURCE_DIR}/Compiler/include
    ${luau_SOURCE_DIR}/Ast/include
)

target_link_libraries(luau_runtime PRIVATE
    Luau.VM
    Luau.Compiler
    Luau.Ast
)
