//
// Created by michele on 27/07/2020.
//

#include <google/protobuf/stubs/common.h>
#include "Thread_guard.h"

/**
 * thread guard constructor
 *
 * @param t_ thread to join on
 * @param stop_ atomic boolean used to signal the end of program
 *
 * @author Michele Crepaldi s269551
 */
Thread_guard::Thread_guard(std::thread &t_, std::atomic<bool>& stop_) : t(t_), stop(stop_) {
}

/**
 * thread guard destructor; it signals the thread to stop and waits it (it also shuts down the protobuf library)
 *
 * @author Michele Crepaldi s269551
 */
Thread_guard::~Thread_guard() {
    //tell the connection thread to stop
    stop.store(true);
    //then join on it
    if(t.joinable())
        t.join();

    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
}
