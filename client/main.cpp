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
#include "Socket.h"

#define EVENT_QUEUE_SIZE 20 //dimension of the event queue, a.k.a. how many events can be putted in queue at the same time
#define SECONDS_BETWEEN_RECONNECTIONS 10

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

    if(argc != 4){
        std::cerr << "Error: format is [" << argv[0] << "] [directory to watch] [server ip address] [server port]" << std::endl;
        exit(1);
    }

    // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
    FileSystemWatcher fw{argv[1], std::chrono::milliseconds(5000)};

    // create a circular vector instance that will contain all the events happened
    Circular_vector<Event> eventQueue(EVENT_QUEUE_SIZE);
    std::string server_ip = argv[2];
    std::string server_port = argv[3];

    // initialize the communication thread (thread that will communicate with the server) and an atomic boolean to make it stop
    std::atomic<bool> thread_stop = false;
    std::thread communication_thread([&thread_stop, &eventQueue, server_ip, server_port](){
        // this thread will loop until it will be told to stop
        while(!thread_stop.load()){
            try{
                struct sockaddr_in server_address = Socket::composeAddress(server_ip, server_port);

                //initialize socket
                Socket client_socket;

                //connect with server
                client_socket.connect(&server_address, sizeof(server_address));

                //TODO authenticate user

                //after connection and authentication then iteratively do these things;
                //if any connection error occurs then redo connection and authentication
                while(!thread_stop.load()) {
                    //extract (front) event from event queue
                    Event e = eventQueue.front();

                    //generate probe request
                    Message probeMessage = e.getProbe();
                    Message optionalAdditionalMesssage;

                    //send request to server
                    client_socket.write(probeMessage.getMessage(), probeMessage.getSize(), 0);

                    //receive server response
                    unsigned long header_length;
                    client_socket.read(reinterpret_cast<char *>(&header_length), sizeof(unsigned long), 0);
                    char response[header_length];
                    client_socket.read(response,header_length,0);
                    messageType resType = Event::getType(response);

                    //evaluate what to do
                    if(e.getElement().is_regular_file()){
                        switch(e.getStatus()) {
                            case FileSystemStatus::created:
                                std::cout << "File created: " << e.getElement().getPath() << "response from server: ";
                                //file created
                                switch(resType){
                                    case messageType::OK:
                                        std::cout << "OK" << std::endl;
                                        //if the file already exists then do nothing
                                        break;
                                    case messageType::NO:
                                        std::cout << "NO" << std::endl;
                                        //if the file does not exist in the server then create (sending) it
                                        optionalAdditionalMesssage = e.getCreateHeader();
                                        client_socket.write(optionalAdditionalMesssage.getMessage(), optionalAdditionalMesssage.getSize(), 0);

                                        //TODO open file and iteratively send it to the server

                                        break;
                                    case messageType::ER:
                                        std::cout << "ER" << std::endl;
                                        //in case of eventual server errors (not connection ones)
                                        //TODO implement error management (possibly retrying)
                                        break;
                                    case messageType::CV:
                                        std::cout << "CV" << std::endl;
                                        //in case of a change version response then change version and retry
                                        //TODO decide if it is worth to implement this or not
                                        break;
                                    default:
                                        std::cout << "Error! Unknown message type." << std::endl;
                                }
                                break;
                            case FileSystemStatus::modified:
                                std::cout << "File modified: " << e.getElement().getPath() << std::endl;
                                //file modified
                                switch(resType){
                                    case messageType::OK:
                                        std::cout << "OK" << std::endl;
                                        //if the file already exists then edit (sending) it.
                                        optionalAdditionalMesssage = e.getEditHeader();
                                        client_socket.write(optionalAdditionalMesssage.getMessage(), optionalAdditionalMesssage.getSize(), 0);

                                        //TODO open file and iteratively send it to the server
                                        break;
                                    case messageType::NO:
                                        std::cout << "NO" << std::endl;
                                        //if the file does not exist in the server then send it
                                        optionalAdditionalMesssage = e.getCreateHeader();
                                        client_socket.write(optionalAdditionalMesssage.getMessage(), optionalAdditionalMesssage.getSize(), 0);

                                        //TODO open file and iteratively send it to the server

                                        break;
                                    case messageType::ER:
                                        std::cout << "ER" << std::endl;
                                        //in case of eventual server errors (not connection ones)
                                        //TODO implement error management (possibly retrying)
                                        break;
                                    case messageType::CV:
                                        std::cout << "CV" << std::endl;
                                        //in case of a change version response then change version and retry
                                        //TODO decide if it is worth to implement this or not
                                        break;
                                    default:
                                        std::cout << "Error! Unknown message type." << std::endl;
                                }
                                break;
                            case FileSystemStatus::deleted:
                                std::cout << "File deleted: " << e.getElement().getPath() << std::endl;
                                //file deleted
                                switch(resType){
                                    case messageType::OK:
                                        std::cout << "OK" << std::endl;
                                        //if the file already exists then delete it
                                        optionalAdditionalMesssage = e.getDeleteHeader();
                                        client_socket.write(optionalAdditionalMesssage.getMessage(), optionalAdditionalMesssage.getSize(), 0);
                                        break;
                                    case messageType::NO:
                                        std::cout << "NO" << std::endl;
                                        //if the file does not exist in the server then do nothing.
                                        break;
                                    case messageType::ER:
                                        std::cout << "ER" << std::endl;
                                        //in case of eventual server errors (not connection ones)
                                        //TODO implement error management (possibly retrying)
                                        break;
                                    case messageType::CV:
                                        std::cout << "CV" << std::endl;
                                        //in case of a change version response then change version and retry
                                        //TODO decide if it is worth to implement this or not
                                        break;
                                    default:
                                        std::cout << "Error! Unknown message type." << std::endl;
                                }
                                break;
                            default:
                                std::cout << "Error! Unknown file status." << std::endl;
                        }
                    }
                    else if(e.getElement().is_directory()){
                        switch(e.getStatus()) {
                            case FileSystemStatus::created:
                                std::cout << "Directory created: " << e.getElement().getPath() << std::endl;
                                //directory created
                                switch(resType){
                                    case messageType::OK:
                                        std::cout << "OK" << std::endl;
                                        //if the directory already exists then do nothing.
                                        break;
                                    case messageType::NO:
                                        std::cout << "NO" << std::endl;
                                        //if the directory does not exist in the server then create it
                                        optionalAdditionalMesssage = e.getCreateHeader();
                                        client_socket.write(optionalAdditionalMesssage.getMessage(), optionalAdditionalMesssage.getSize(), 0);
                                        break;
                                    case messageType::ER:
                                        std::cout << "ER" << std::endl;
                                        //in case of eventual server errors (not connection ones)
                                        //TODO implement error management (possibly retrying)
                                        break;
                                    case messageType::CV:
                                        std::cout << "CV" << std::endl;
                                        //in case of a change version response then change version and retry
                                        //TODO decide if it is worth to implement this or not
                                        break;
                                    default:
                                        std::cout << "Error! Unknown message type." << std::endl;
                                }
                                break;
                            /*case FileSystemStatus::modified:
                                //std::cout << "Directory modified: " << e.getElement().getPath() << std::endl;
                                break;*/
                            case FileSystemStatus::deleted:
                                std::cout << "Directory deleted: " << e.getElement().getPath() << std::endl;
                                //directory deleted
                                switch(resType){
                                    case messageType::OK:
                                        std::cout << "OK" << std::endl;
                                        //if the directory already exists then delete it.
                                        optionalAdditionalMesssage = e.getDeleteHeader();
                                        client_socket.write(optionalAdditionalMesssage.getMessage(), optionalAdditionalMesssage.getSize(), 0);
                                        break;
                                    case messageType::NO:
                                        std::cout << "NO" << std::endl;
                                        //if the directory does not exist in the server then do nothing
                                        break;
                                    case messageType::ER:
                                        std::cout << "ER" << std::endl;
                                        //in case of eventual server errors (not connection ones)
                                        //TODO implement error management (possibly retrying)
                                        break;
                                    case messageType::CV:
                                        std::cout << "CV" << std::endl;
                                        //in case of a change version response then change version and retry
                                        //TODO decide if it is worth to implement this or not
                                        break;
                                    default:
                                        std::cout << "Error! Unknown message type." << std::endl;
                                }
                                break;
                            default:
                                std::cout << "Error! Unknown file status." << std::endl;
                        }
                    }
                    else{
                        std::cout << "change to an unsupported type." << std::endl;
                    }

                    //in case everything went smoothly then pop event from event queue
                    eventQueue.pop();
                }
            }
            catch (std::runtime_error &err){
                //error in connection; retry later; wait for x seconds
                std::cout << "Connection error. Retry in " << SECONDS_BETWEEN_RECONNECTIONS << " seconds." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(SECONDS_BETWEEN_RECONNECTIONS));
            }
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