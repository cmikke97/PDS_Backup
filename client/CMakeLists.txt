cmake_minimum_required(VERSION 3.15)
project(client)

set(CMAKE_CXX_STANDARD 17)

add_executable(client main.cpp FileSystemWatcher.cpp FileSystemWatcher.h Directory_entry.cpp Directory_entry.h)