cmake_minimum_required(VERSION 3.17)
project(client)

set(CMAKE_CXX_STANDARD 17)

#set some variables
set(PROJECT_NAME client)
set(SOURCE_FILES main.cpp FileSystemWatcher.cpp FileSystemWatcher.h Directory_entry.cpp Directory_entry.h Event.cpp
        Event.h Thread_guard.cpp Thread_guard.h ProtocolManager.cpp ProtocolManager.h Database.cpp Database.h)
set(MYLIBRARY ../myLibraries/Socket.cpp ../myLibraries/Socket.h ../myLibraries/Hash.cpp ../myLibraries/Hash.h
        ../myLibraries/TSCircular_vector.cpp ../myLibraries/TSCircular_vector.h)

#now we want to include OpenSSL and protocol buffers
if (CYGWIN) #if on windows

    #converts a native <path> into a cmake-style path with forward-slashes (/); in this case convert the OPENSSL_ROOT env variable to a cmake-style path representation
    file(TO_CMAKE_PATH $ENV{OPENSSL_ROOT} OPENSSL_ROOT_DIR)
    #replace the sequence "C/" to "/cygdrive/c/"
    string(REGEX REPLACE "C/" "/cygdrive/c/" OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR})
    #set some variables (directories)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_ROOT_DIR}/bin/libcrypto-1_1-x64.dll)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    #find OpenSSL package using previoulsy defined variables
    find_package(OpenSSL REQUIRED)

    #print a simple message
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION} on windows")

    #converts a native <path> into a cmake-style path with forward-slashes (/); in this case convert the OPENSSL_ROOT env variable to a cmake-style path representation
    file(TO_CMAKE_PATH $ENV{CYGWIN_ROOT} PROTOC_ROOT_DIR)
    #replace the sequence "C/" to "/cygdrive/c/"
    string(REGEX REPLACE "C/" "/cygdrive/c/" PROTOC_ROOT_DIR ${PROTOC_ROOT_DIR})
    #set some variables (directories)
    set(Protobuf_PROTOC_EXECUTABLE ${PROTOC_ROOT_DIR}/bin/protoc.exe)
    set(Protobuf_LIBRARY ${PROTOC_ROOT_DIR}/lib/libprotobuf.dll.a)
    set(Protobuf_INCLUDE_DIR ${PROTOC_ROOT_DIR}/usr/include)
    #find Protocol buffers package using previoulsy defined variables
    find_package(Protobuf)

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

    #link OpenSSL and Protobuf libraries to the project
    target_link_libraries(${PROJECT_NAME} OpenSSL::Crypto Protobuf)

else () #if on unix
    #set some cmake flags to properly include pthreads (for std::thread)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread -lprotobuf")

    find_package(Protobuf)

    #print a simple message
    message(STATUS "Using Protocol buffers ${Protobuf_VERSION} on linux")

    #include protocol buffers directories
    include_directories(${Protobuf_INCLUDE_DIRS})

    #include current binary dir
    include_directories(${CMAKE_CURRENT_BINARY_DIR})

    #generate the messages object from messages.proto file (it is done every time you compile the project..)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ../myProtos/messages.proto)

    #add all source files to project executable + message objects automatically generated
    add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${MYLIBRARY} ${PROTO_SRCS} ${PROTO_HDRS})

    #link OpenSSL and Protobuf libraries to the project
    target_link_libraries(${PROJECT_NAME} ssl crypto Protobuf)

    #print a simple message
    message(STATUS "Using OpenSSL on linux")

endif ()

find_package(SQLite3)
include_directories(${SQLite3_INCLUDE_DIRS})
target_link_libraries (${PROJECT_NAME} SQLite::SQLite3)
message(STATUS "Using SQLite3 version ${SQLite3_VERSION}")