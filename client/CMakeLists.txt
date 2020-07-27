cmake_minimum_required(VERSION 3.15)
project(client)

set(CMAKE_CXX_STANDARD 17)
set(SOURCE_FILES main.cpp FileSystemWatcher.cpp FileSystemWatcher.h Directory_entry.cpp Directory_entry.h Event.cpp Event.h Circular_vector.cpp Circular_vector.h Thread_guard.cpp Thread_guard.h)

add_executable(client ${SOURCE_FILES})