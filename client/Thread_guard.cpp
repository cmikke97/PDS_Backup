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
 * @param t thread to join on
 * @param stop atomic boolean used to signal to the thread to stop
 *
 * @author Michele Crepaldi s269551
 */
client::Thread_guard::Thread_guard(std::thread &t, std::atomic<bool>& stop) : _t(t), _stop(stop) {
}

/**
 * Thread_guard class destructor; it signals the thread to stop and waits for it
 * (it also shuts down the protobuf library)
 *
 * @author Michele Crepaldi s269551
 */
client::Thread_guard::~Thread_guard() {
    //tell the communication thread to stop
    _stop.store(true);

    //then join on it
    if(_t.joinable())
        _t.join();

    //delete all global objects allocated by libprotobuf
    google::protobuf::ShutdownProtobufLibrary();
}
