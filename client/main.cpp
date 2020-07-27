//
// Created by michele on 25/07/2020.
//

#include <chrono>
#include <string>
#include <iostream>
#include <atomic>
#include "FileSystemWatcher.h"
#include "Directory_entry.h"
#include "Event.h"
#include "Circular_vector.h"
#include "Thread_guard.h"

#define EVENT_QUEUE_SIZE 20 //dimension of the event queue, a.k.a. how many events can be putted in queue at the same time

/**
 * the client main function
 *
 * @param argc command argument number
 * @param argv command arguments
 * @return exit value
 *
 * @author Michele Crepaldi s269551
 */
int main(int argc, char **argv) {

    if(argc != 3){
        std::cerr << "Error: format is [" << argv[0] << "] [directory to watch] [server ip address]" << std::endl;
        exit(1);
    }

    // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
    FileSystemWatcher fw{argv[1], std::chrono::milliseconds(5000)};

    // create a circular vector instance that will contain all the events happened
    Circular_vector<Event> eventQueue(EVENT_QUEUE_SIZE);

    // initialize the communication thread (thread that will communicate with the server) and an atomic boolean to make it stop
    std::atomic<bool> thread_stop = false;
    std::thread communication_thread([&thread_stop, &eventQueue](){
        // this thread will loop until it will be told to stop
        while(!thread_stop.load()){
            //TODO initialize socket

            //TODO connect with server

            //TODO authenticate user

            //TODO extract (front) event from event queue

            //TODO generate request

            //TODO send request to server

            //TODO receive server response

            //TODO evaluate what to do

            //TODO send optional response to server

            //TODO receive optional server response

            //TODO in case everything went smoothly then pop event from event queue
        }
    });
    // use thread guard to signal to the communication thread to stop and wait for it in case we exit the main
    Thread_guard tg_communication(communication_thread, thread_stop);

    // Start monitoring a folder for changes and (in case of changes)
    // run a user provided lambda function
    fw.start([&eventQueue](Directory_entry& element_to_watch, FileSystemStatus status) -> bool {
        //it returns true if the object was successfully pushed, false otherwise; in any case it returns immediately with no waiting.
        //This is done is such way as to not be blocked waiting for the queue to be not full.
        //So elements not pushed successfully won't be added to the already watched ones and this process will be repeated after some time
        return eventQueue.tryPush(std::move(Event(element_to_watch,status)));
    });
}