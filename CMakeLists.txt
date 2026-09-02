cmake_minimum_required(VERSION 3.16)
project(jitcommon CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings are errors everywhere, but the spelling differs. Naming MSVC's
# equivalent rather than dropping the flags on Windows keeps the strictest
# platform from becoming the least checked one.
if(MSVC)
  set(JC_WARN_FLAGS /W4 /WX)
else()
  set(JC_WARN_FLAGS -Wall -Wextra -Werror)
endif()

add_library(jitcommon STATIC
  src/jitcommon/code_memory.cpp
  src/jitcommon/block_cache.cpp)
target_include_directories(jitcommon PUBLIC src/jitcommon)
target_compile_options(jitcommon PRIVATE ${JC_WARN_FLAGS})

# Tests belong to THIS project, not to whoever consumes it. A repo that pulls
# jit-common in with add_subdirectory would otherwise inherit these tests into
# its own ctest run, where they check jit-common's sources against a compile
# database that consumer's build never generates -- a failure in the consumer's
# suite that says nothing about the consumer.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  return()
endif()

enable_testing()

add_executable(test_code_memory tests/test_code_memory.cpp)
target_link_libraries(test_code_memory PRIVATE jitcommon)
target_compile_options(test_code_memory PRIVATE ${JC_WARN_FLAGS})
add_test(NAME test_code_memory COMMAND test_code_memory)

add_executable(test_block_cache tests/test_block_cache.cpp)
target_link_libraries(test_block_cache PRIVATE jitcommon)
target_compile_options(test_block_cache PRIVATE ${JC_WARN_FLAGS})
add_test(NAME test_block_cache COMMAND test_block_cache)

# Style is a test, not a convention nobody runs. AGENTS.md states clang-format
# and clang-tidy are enforced by the suite; this is that enforcement. The script
# refuses rather than passes when a tool or the compile database is missing.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME style
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_style.py
          --build ${CMAKE_BINARY_DIR})
