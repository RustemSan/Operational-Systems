cmake_minimum_required(VERSION 3.22.1)
project(hw1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/x86_64-linux-gnu/)

add_executable(hw1
        solution.cpp
        sample_tester.cpp
        common.h
)

target_include_directories(hw1 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(hw1 PRIVATE
        libprogtest_solver.a
)
