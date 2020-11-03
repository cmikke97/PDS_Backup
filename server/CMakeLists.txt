cmake_minimum_required(VERSION 3.17)
project(server)

set(CMAKE_CXX_STANDARD 17)

#set some variables
set(SOURCE_FILES main.cpp Thread_guard.h Thread_guard.cpp ProtocolManager.h ProtocolManager.cpp PWD_Database.cpp
        PWD_Database.h Database.h Database.cpp)
set(MYLIBRARY ../myLibraries/Socket.cpp ../myLibraries/Socket.h ../myLibraries/Hash.cpp ../myLibraries/Hash.h
        ../myLibraries/TSCircular_vector.cpp ../myLibraries/TSCircular_vector.h ../myLibraries/Directory_entry.cpp
        ../myLibraries/Directory_entry.h ../myLibraries/RandomNumberGenerator.h ../myLibraries/RandomNumberGenerator.cpp)

#now we want to include wolfSSL, protocol buffers and sqlite3
if (CYGWIN) #if on windows

    #convert a native <path> into a cmake-style path with forward-slashes (/); in this case convert the CYGWIN_ROOT env variable to a cmake-style path representation
    file(TO_CMAKE_PATH $ENV{CYGWIN_ROOT} WSSL_ROOT_DIR)
    #replace the sequence "C/" to "/cygdrive/c/"
    string(REGEX REPLACE "C/" "/cygdrive/c/" WSSL_ROOT_DIR ${WSSL_ROOT_DIR})

    set(LIB_PATH "${WSSL_ROOT_DIR}/usr/local")

    #print a simple message
    message(STATUS "Using wolfSSL on windows")

    #convert a native <path> into a cmake-style path with forward-slashes (/); in this case convert the CYGWIN_ROOT env variable to a cmake-style path representation
    file(TO_CMAKE_PATH $ENV{CYGWIN_ROOT} PROTOC_ROOT_DIR)
    #replace the sequence "C/" to "/cygdrive/c/"
    string(REGEX REPLACE "C/" "/cygdrive/c/" PROTOC_ROOT_DIR ${PROTOC_ROOT_DIR})
    #set some variables (directories)
    set(Protobuf_PROTOC_EXECUTABLE ${PROTOC_ROOT_DIR}/bin/protoc.exe)
    set(Protobuf_LIBRARY ${PROTOC_ROOT_DIR}/lib/libprotobuf.dll.a)
    set(Protobuf_INCLUDE_DIR ${PROTOC_ROOT_DIR}/usr/include)

else () #if on unix
    set(LIB_PATH "/usr/local")

    #set some cmake flags to properly include pthreads (for std::thread)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")

    #print a simple message
    message(STATUS "Using wolfSSL on linux")

endif ()

#set some variables
set(CFLAGS "-I${LIB_PATH}/include")
set(LIBS "-L${LIB_PATH}/lib -lm")
set(STATIC_LIB "${LIB_PATH}/lib/libwolfssl.la")
set(DYN_LIB -lwolfssl)

#find Protocol buffers package using previously defined variables
find_package(Protobuf REQUIRED)

#print a simple message
message(STATUS "Using Protocol buffers ${Protobuf_VERSION} on windows")

#include protocol buffers directories
include_directories(${Protobuf_INCLUDE_DIRS})

#include current binary dir
include_directories(${CMAKE_CURRENT_BINARY_DIR})

#generate the messages object from messages.proto file (it is done every time you compile the project..)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../myProtos/messages.proto)

#add all source files to project executable + message objects automatically generated
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${MYLIBRARY} ${PROTO_SRCS} ${PROTO_HDRS})

#link wolfSSL and Protobuf libraries to the project
target_link_libraries(${PROJECT_NAME} ${CFLAGS} ${LIBS} ${DYN_LIB} ${PROTOBUF_LIBRARIES})

#find sqlite3
find_package(SQLite3)
#include its directories
include_directories(${SQLite3_INCLUDE_DIRS})
#link sqlite3
target_link_libraries (${PROJECT_NAME} SQLite::SQLite3)
message(STATUS "Using SQLite3 version ${SQLite3_VERSION}")