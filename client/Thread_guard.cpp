//
// Created by michele on 27/07/2020.
//

#include "Thread_guard.h"

Thread_guard::Thread_guard(std::thread &t_, std::atomic<bool>& stop_) : t(t_), stop(stop_) {
}

Thread_guard::~Thread_guard() {
    stop.store(true);
    if(t.joinable())
        t.join();
}
