cmake_minimum_required(VERSION 3.15)
project(client)

set(CMAKE_CXX_STANDARD 17)

#set some variables
set(PROJECT_NAME client)
set(SOURCE_FILES main.cpp FileSystemWatcher.cpp FileSystemWatcher.h Directory_entry.cpp Directory_entry.h Event.cpp Event.h
        Circular_vector.cpp Circular_vector.h Thread_guard.cpp Thread_guard.h Hash.cpp Hash.h Message.cpp Message.h)

#now we want to include OpenSSL and protocol buffers
if (CYGWIN) #if on windows

    #converts a native <path> into a cmake-style path with forward-slashes (/); in this case convert the OPENSSL_ROOT env variable to a cmake-style path representation
    file(TO_CMAKE_PATH $ENV{OPENSSL_ROOT} OPENSSL_ROOT_DIR)
    #replace the sequence "C/" to "C:/"
    string(REGEX REPLACE "C/" "C:/" OPENSSL_ROOT_DIR ${OPENSSL_ROOT_DIR})
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
    #replace the sequence "C/" to "C:/"
    string(REGEX REPLACE "C/" "C:/" PROTOC_ROOT_DIR ${PROTOC_ROOT_DIR})
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
    add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${PROTO_SRCS} ${PROTO_HDRS})

    #link OpenSSL and Protobuf libraries to the project
    target_link_libraries(${PROJECT_NAME} OpenSSL::Crypto ${Protobuf_LIBRARIES})

else () #if on unix
    #set some cmake flags to properly include pthreads (for std::thread)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread -lprotobuf")

    #target the OpenSSL crypto libraries to the project
    target_link_libraries(${PROJECT_NAME} PUBLIC ssl crypto)

    #print a simple message
    message(STATUS "Using OpenSSL on linux")

endif ()

#add my custom libraries and the protocol buffers library to myLibrary
add_library(myLibrary STATIC  "../myLibraries/Socket.cpp" "../myLibraries/Socket.h")

#target my custom library to the project
target_link_libraries(${PROJECT_NAME} myLibrary)