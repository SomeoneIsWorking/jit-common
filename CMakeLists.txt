cmake_minimum_required(VERSION 3.16)
project(jitcommon CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(jitcommon STATIC
  src/jitcommon/code_memory.cpp)
target_include_directories(jitcommon PUBLIC src/jitcommon)
target_compile_options(jitcommon PRIVATE -Wall -Wextra -Werror)

enable_testing()

add_executable(test_code_memory tests/test_code_memory.cpp)
target_link_libraries(test_code_memory PRIVATE jitcommon)
target_compile_options(test_code_memory PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_code_memory COMMAND test_code_memory)
