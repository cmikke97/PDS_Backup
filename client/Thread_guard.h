//
// Created by Michele Crepaldi s269551 on 27/07/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#ifndef CLIENT_THREAD_GUARD_H
#define CLIENT_THREAD_GUARD_H

#include <thread>
#include <atomic>


/**
 * PDS_Backup client namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace client {
    /**
     * Thread_guard class. It is used to join the communication thread
     *
     * @author Michele Crepaldi s269551
     */
    class Thread_guard {
    public:
        Thread_guard(const Thread_guard &) = delete;    //copy constructor deleted
        Thread_guard& operator=(const Thread_guard &) = delete; //assignment deleted
        Thread_guard(Thread_guard &&) = delete; //move constructor deleted
        Thread_guard& operator=(Thread_guard &&) = delete;  //move assignment deleted

        Thread_guard(std::thread &t, std::atomic<bool> &stop);    //constructor
        ~Thread_guard();    //destructor

    private:
        std::thread &_t;    //reference to the communication thread
        std::atomic<bool> &_stop;   //reference to the atomic boolean used to stop the communication thread
    };
}


#endif //CLIENT_THREAD_GUARD_H
