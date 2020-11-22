//
// Created by Michele Crepaldi s269551 on 27/07/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#include "Thread_guard.h"

#include <google/protobuf/stubs/common.h>


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Thread_guard class methods
 */

/**
 * Thread_guard class constructor
 *
 * @param t vector of all threads to join on
 * @param stop atomic boolean used to signal to the threads to stop
 *
 * @author Michele Crepaldi s269551
 */
server::Thread_guard::Thread_guard(std::vector<std::thread> &t, std::atomic<bool> &stop) : _tVector(t), _stop(stop) {
}

/**
 * Thread_guard class destructor; it signals the threads to stop and waits for them
 *  (it also shuts down the protobuf library)
 *
 * @author Michele Crepaldi s269551
 */
server::Thread_guard::~Thread_guard() {
    //tell the single server threads to stop
    _stop.store(true);

    //then join on all threads
    for(auto &t : _tVector)
        if(t.joinable())
            t.join();

    //delete all global objects allocated by libprotobuf
    google::protobuf::ShutdownProtobufLibrary();
}
