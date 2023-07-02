cmake_minimum_required(VERSION 3.23)
project(labwork_9_grumbletumbles)

set(CMAKE_CXX_STANDARD 20)

include_directories(lib)

add_executable(labwork_9_grumbletumbles
        bin/main.cpp
        lib/MemoryPoolAllocator.h)
