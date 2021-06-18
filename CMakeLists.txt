cmake_minimum_required(VERSION 3.16)
project(valorant_input_log)

set(CMAKE_CXX_STANDARD 14)

add_executable(valorant_input_log src/main.cpp src/format_types.h src/locking_queue.hpp)