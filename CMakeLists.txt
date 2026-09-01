cmake_minimum_required(VERSION 3.16)
project(jitcommon CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(jitcommon STATIC
  src/jitcommon/code_memory.cpp
  src/jitcommon/block_cache.cpp)
target_include_directories(jitcommon PUBLIC src/jitcommon)
target_compile_options(jitcommon PRIVATE -Wall -Wextra -Werror)

enable_testing()

add_executable(test_code_memory tests/test_code_memory.cpp)
target_link_libraries(test_code_memory PRIVATE jitcommon)
target_compile_options(test_code_memory PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_code_memory COMMAND test_code_memory)

add_executable(test_block_cache tests/test_block_cache.cpp)
target_link_libraries(test_block_cache PRIVATE jitcommon)
target_compile_options(test_block_cache PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_block_cache COMMAND test_block_cache)

# Style is a test, not a convention nobody runs. AGENTS.md states clang-format
# and clang-tidy are enforced by the suite; this is that enforcement. The script
# refuses rather than passes when a tool or the compile database is missing.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME style
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_style.py
          --build ${CMAKE_BINARY_DIR})
