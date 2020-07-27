cmake_minimum_required(VERSION 3.15)
project(client)

set(CMAKE_CXX_STANDARD 17)
set(SOURCE_FILES main.cpp FileSystemWatcher.cpp FileSystemWatcher.h Directory_entry.cpp Directory_entry.h Event.cpp Event.h Circular_vector.cpp Circular_vector.h Thread_guard.cpp Thread_guard.h)

set(OPENSSL_ROOT_DIR C:/Program Files/OpenSSL-Win64)
set(OPENSSL_CRYPTO_LIBRARY C:/Program Files/OpenSSL-Win64/lib)
set(OPENSSL_INCLUDE_DIR C:/Program Files/OpenSSL-Win64/include/openssl)
find_package(OpenSSL REQUIRED)

add_executable(client ${SOURCE_FILES})
target_link_libraries(client OpenSSL::SSL OpenSSL::Crypto)