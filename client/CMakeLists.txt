cmake_minimum_required(VERSION 3.15)
project(client)

set(CMAKE_CXX_STANDARD 17)
set(SOURCE_FILES main.cpp FileSystemWatcher.cpp FileSystemWatcher.h Directory_entry.cpp Directory_entry.h Event.cpp Event.h
        Circular_vector.cpp Circular_vector.h Thread_guard.cpp Thread_guard.h Hash.cpp Hash.h Socket.h Socket.cpp Message.cpp Message.h)
set(PROJECT_NAME client)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

if (CYGWIN)
    #if on windows
    file(TO_CMAKE_PATH $ENV{OPENSSL_ROOT} OPENSSL_ROOT_DIR)
    string(REGEX REPLACE "C" "C:" OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR})
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_ROOT_DIR}/bin/libcrypto-1_1-x64.dll)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    find_package(OpenSSL REQUIRED)

    message(STATUS "Using OpenSSL ${OPENSSL_VERSION} on windows")

    target_link_libraries(${PROJECT_NAME} OpenSSL::Crypto)
endif (CYGWIN)

if (UNIX)
    #if on unix
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")

    target_link_libraries(${PROJECT_NAME} PUBLIC ssl crypto)
    message(STATUS "Using OpenSSL on linux")
endif (UNIX)