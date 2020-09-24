//
// Created by michele on 25/07/2020.
//

#include <chrono>
#include <string>
#include <iostream>
#include <atomic>
#include "FileSystemWatcher.h"
#include "Event.h"
#include "Circular_vector.h"
#include "Thread_guard.h"
#include "../myLibraries/Socket.h"
#include "messages.pb.h"

#define EVENT_QUEUE_SIZE 20 //dimension of the event queue, a.k.a. how many events can be putted in queue at the same time
#define SECONDS_BETWEEN_RECONNECTIONS 10
#define TIMEOUT 300 //seconds to wait before client-server connection timeout (5 minutes)

void communicate(std::atomic<bool> &, Circular_vector<Event> &, const std::string &, const std::string &);

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
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (argc != 4) {
        std::cerr << "Error: format is [" << argv[0] << "] [directory to watch] [server ip address] [server port]"
                  << std::endl;
        exit(1);
    }

    // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
    FileSystemWatcher fw{argv[1], std::chrono::milliseconds(5000)};

    // create a circular vector instance that will contain all the events happened
    Circular_vector<Event> eventQueue(EVENT_QUEUE_SIZE);

    //extract the arguments
    std::string server_ip = argv[2];
    std::string server_port = argv[3];

    // initialize the communication thread (thread that will communicate with the server) and an atomic boolean to make it stop
    std::atomic<bool> thread_stop = false;
    std::thread communication_thread(communicate, std::ref(thread_stop), std::ref(eventQueue), server_ip, server_port);

    // use thread guard to signal to the communication thread to stop and wait for it in case we exit the main
    Thread_guard tg_communication(communication_thread, thread_stop);

    // Start monitoring a folder for changes and (in case of changes) run a user provided lambda function
    fw.start([&eventQueue](Directory_entry &element, FileSystemStatus status) -> bool {
        //it returns true if the object was successfully pushed, false otherwise; in any case it returns immediately with no waiting.
        //This is done is such way as to not be blocked waiting for the queue to be not full.
        //So elements not pushed successfully won't be added to the already watched ones and this process will be repeated after some time
        return eventQueue.tryPush(std::move(Event(element, status)));
    });
}

void communicate(std::atomic<bool> &thread_stop, Circular_vector<Event> &eventQueue, const std::string &server_ip,
                 const std::string &server_port) {
    messages::ClientMessage clientMessage;
    messages::ServerMessage serverMessage;
    std::string client_temp, server_temp;
    int errorCode;

    // this thread will loop until it will be told to stop
    while (!thread_stop.load()) {
        try {
            struct sockaddr_in server_address = Socket::composeAddress(server_ip, server_port);

            //initialize socket
            Socket client_socket;

            //extract (front) event from event queue (blocking wait)
            Event e = eventQueue.front();

            //connect with server
            client_socket.connect(&server_address, sizeof(server_address));

            //TODO authenticate user

            //after connection and authentication then iteratively do these things;
            //if any connection error occurs then redo connection and authentication
            while (!thread_stop.load()) {
                //evaluate what to do

                if (e.getElement().is_regular_file()) {   //if the element is a file
                    switch (e.getStatus()) {
                        case FileSystemStatus::created: //file created
                            std::cout << "File created: " << e.getElement().getPath() << "response from server: ";

                            //create message
                            clientMessage.set_type(messages::ClientMessage_Type_STOR);
                            clientMessage.set_path(e.getElement().getPath());
                            clientMessage.set_filesize(e.getElement().getSize());
                            clientMessage.set_lastwritetime(e.getElement().getLastWriteTime());
                            clientMessage.set_hash(e.getElement().getHash().getValue().first,
                                                   e.getElement().getHash().getValue().second);
                            break;

                        case FileSystemStatus::deleted: //file deleted
                            std::cout << "File deleted: " << e.getElement().getPath() << std::endl;

                            //create message
                            clientMessage.set_type(messages::ClientMessage_Type_DELE);
                            clientMessage.set_path(e.getElement().getPath());
                            clientMessage.set_hash(e.getElement().getHash().getValue().first,
                                                   e.getElement().getHash().getValue().second);
                            break;

                        default:
                            std::cerr << "Error! Unknown file status." << std::endl;
                    }
                } else if (e.getElement().is_directory()) { //if the element is a directory
                    switch (e.getStatus()) {
                        case FileSystemStatus::created: //directory created
                            std::cout << "Directory created: " << e.getElement().getPath() << std::endl;

                            //create message
                            clientMessage.set_type(messages::ClientMessage_Type_MKD);
                            clientMessage.set_path(e.getElement().getPath());
                            break;

                        case FileSystemStatus::deleted: //directory deleted
                            std::cout << "Directory deleted: " << e.getElement().getPath() << std::endl;

                            //create message
                            clientMessage.set_type(messages::ClientMessage_Type_RMD);
                            clientMessage.set_path(e.getElement().getPath());
                            break;

                        default:
                            std::cerr << "Error! Unknown file status." << std::endl;
                    }
                } else {
                    std::cerr << "change to an unsupported type." << std::endl;
                }

                //if the event is of a supported type
                if (e.getElement().is_regular_file() || e.getElement().is_directory()) {
                    //compute message
                    //clientMessage.SerializeToString(&message);
                    client_temp = clientMessage.SerializeAsString();

                    //send message to server
                    client_socket.stringWrite(client_temp, 0);

                    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
                    clientMessage.Clear();

                    server_temp = client_socket.stringRead(0);

                    serverMessage.ParseFromString(server_temp);
                    std::cout << serverMessage.type();

                    switch (serverMessage.type()) {
                        case messages::ServerMessage_Type_OK:
                            // do nothing. The command succeded
                            std::cout << "Command for " << e.getElement().getPath() << " has been sent to server"
                                      << std::endl;
                            break;
                        case messages::ServerMessage_Type_SEND:
                            //send file

                            //check if the event is about the creation of a file
                            if (!e.getElement().is_regular_file() || e.getStatus() != FileSystemStatus::created) {
                                break; //exit the switch and go on (do not send anything)
                            }

                            //TODO read and send file
                            break;
                        case messages::ServerMessage_Type_ERR:
                            //retrieve error code
                            errorCode = serverMessage.errcode();
                            std::cerr << "Error: code " << errorCode << std::endl;

                            //based on error code handle it
                            //TODO handle error
                            break;
                        default:
                            break;
                    }
                }

                //in case everything went smoothly then pop event from event queue
                eventQueue.pop();

                //extract (front) event from event queue; if nothing happens for TIMEOUT seconds then disconnect from server
                std::optional<Event> m = eventQueue.frontWaitFor(
                        TIMEOUT); //it return optionally the next event as value
                if (!m.has_value()) { //if there is no value it means that it timed out!
                    //create message
                    clientMessage.set_type(messages::ClientMessage_Type_QUIT);

                    //compute message
                    client_temp = clientMessage.SerializeAsString();

                    //send message to server
                    client_socket.stringWrite(client_temp, 0);

                    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
                    clientMessage.Clear();
                    break;
                }

                // if an event has occured then assign it to the current event to be handled
                e = m.value();
            }
        }
        catch (std::runtime_error &err) {
            //error in connection; retry later; wait for x seconds
            std::cout << "Connection error. Retry in " << SECONDS_BETWEEN_RECONNECTIONS << " seconds." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(SECONDS_BETWEEN_RECONNECTIONS));

            //TODO handle error
        }
    }
}