//
// Created by michele on 27/07/2020.
//

#ifndef SERVER_THREAD_GUARD_H
#define SERVER_THREAD_GUARD_H

#include <thread>
#include <atomic>
#include <sqlite3.h>

/**
 * thread guard class
 *
 * @author Michele Crepaldi s269551
 */
class Thread_guard {
    std::vector<std::thread> &t;
    std::atomic<bool> &stop;

public:
    Thread_guard(std::vector<std::thread> &t_, std::atomic<bool> &stop_);
    ~Thread_guard();

    Thread_guard(const Thread_guard &) = delete;
    Thread_guard& operator=(const Thread_guard &) = delete;

};


#endif //SERVER_THREAD_GUARD_H
