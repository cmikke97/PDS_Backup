//
// Created by michele on 27/07/2020.
//

#ifndef CLIENT_THREAD_GUARD_H
#define CLIENT_THREAD_GUARD_H

#include <thread>
#include <atomic>

/**
 * thread guard class
 *
 * @author Michele Crepaldi s269551
 */
class Thread_guard {
    std::thread& t;
    std::atomic<bool>& stop;

public:
    Thread_guard(std::thread& t_, std::atomic<bool>& stop_);
    ~Thread_guard();

    Thread_guard(const Thread_guard &) = delete;
    Thread_guard& operator=(const Thread_guard &) = delete;

};


#endif //CLIENT_THREAD_GUARD_H
