//
// Created by Michele Crepaldi s269551 on 27/07/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#ifndef SERVER_THREAD_GUARD_H
#define SERVER_THREAD_GUARD_H

#include <thread>
#include <atomic>
#include <vector>

#include "../myLibraries/Circular_vector.h"
#include "../myLibraries/Socket.h"


/**
 * PDS_Backup server namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace server {
    /**
     * Thread_guard class. It is used to join all the threads
     *
     * @author Michele Crepaldi s269551
     */
    class Thread_guard {
    public:
        Thread_guard(const Thread_guard &) = delete;    //copy constructor deleted
        Thread_guard& operator=(const Thread_guard &) = delete; //assignment deleted
        Thread_guard(Thread_guard &&) = delete; //move constructor deleted
        Thread_guard& operator=(Thread_guard &&) = delete;  //move assignment deleted

        Thread_guard(std::vector<std::thread> &t, Circular_vector<std::pair<std::string, Socket>> &sockets,
                     std::atomic<bool> &stop);   //constructor

        ~Thread_guard();    //destructor

    private:
        std::vector<std::thread> &_tVector;    //reference to the vector of threads
        std::atomic<bool> &_stop;   //reference to the atomic boolean used to stop the threads
        Circular_vector<std::pair<std::string, Socket>> &_sockets;  //sockets circular vector
    };
}


#endif //SERVER_THREAD_GUARD_H
