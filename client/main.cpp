//
// Created by michele on 25/07/2020.
//

#include <chrono>
#include <string>
#include <iostream>
#include <atomic>
#include "FileSystemWatcher.h"
#include "Event.h"
#include "../myLibraries/TSCircular_vector.h"
#include "Thread_guard.h"
#include "../myLibraries/Socket.h"
#include "messages.pb.h"
#include "ProtocolManager.h"

#define EVENT_QUEUE_SIZE 20 //dimension of the event queue, a.k.a. how many events can be putted in queue at the same time
#define SECONDS_BETWEEN_RECONNECTIONS 10 //seconds to wait before client retrying connection after connection lost
#define MAX_CONNECTION_RETRIES 12 //maximum number of times the system will re-try to connect consecutively
#define MAX_SERVER_ERROR_RETRIES 5 //maximum number of times the system will try re-sending a message (and all messages sent after it to maintain the final outcome) after an internal server error
#define MAX_AUTH_ERROR_RETRIES 5 //maximum number of times the system will re-try the authentication after an internal server error
#define TIMEOUT 300 //seconds to wait before client-server connection timeout (5 minutes)
#define SELECT_TIMEOUT 5
#define MAX_RESPONSE_WAITING 1024 //maximum amount of messages that can be sent without response
#define VERSION 1
#define DATABASE_PATH "C:/Users/michele/CLionProjects/PDS_Backup/client/clientDB/clientDB.sqlite"

//TODO get this costants from a configuration file

void communicate(std::atomic<bool> &, std::atomic<bool> &fileWatcher_stop, TSCircular_vector<Event> &, const std::string &, const std::string &, const std::string &, const std::string &);

Database db;

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

    if (argc != 6) {
        std::cerr << "Error: format is [" << argv[0] << "] [directory to watch] [server ip address] [server port] [username] [password]" << std::endl;
        exit(1);
    }

    // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
    FileSystemWatcher fw{argv[1], std::chrono::milliseconds(5000)};

    try {
        //open the database (and if not previously there create also the needed table)
        db.open(DATABASE_PATH);

        //make the filesystem watcher retrieve previously saved data from db
        fw.recoverFromDB(db);
    }
    catch (std::runtime_error &e) {
        //in case of database exceptions show message and return
        std::cerr << e.what() << std::endl;
        return 1;
    }

    // create a circular vector instance that will contain all the events happened
    TSCircular_vector<Event> eventQueue(EVENT_QUEUE_SIZE);

    //extract the arguments
    std::string server_ip = argv[2];
    std::string server_port = argv[3];
    std::string username = argv[4];
    std::string password = argv[5];

    //initialize some atomic boolean to make the different threads stop
    std::atomic<bool> communicationThread_stop = false, fileWatcher_stop = false;

    // initialize the communication thread (thread that will communicate with the server)
    std::thread communication_thread(communicate, std::ref(communicationThread_stop), std::ref(fileWatcher_stop), std::ref(eventQueue), server_ip, server_port, username, password);

    // use thread guard to signal to the communication thread to stop and wait for it in case we exit the main
    Thread_guard tg_communication(communication_thread, communicationThread_stop);

    // Start monitoring a folder for changes and (in case of changes) run a user provided lambda function
    fw.start([&eventQueue](Directory_entry &element, FileSystemStatus status) -> bool {
        //it returns true if the object was successfully pushed, false otherwise; in any case it returns immediately with no waiting.
        //This is done is such way as to not be blocked waiting for the queue to be not full.
        //So elements not pushed successfully won't be added to the already watched ones and this process will be repeated after some time
        return eventQueue.tryPush(std::move(Event(element, status)));
    }, fileWatcher_stop);

    //if we are here then there was an error in the communication thread which caused the termination of the file system watcher
    return 1;
}

/**
 * thread function to handle the client - server interaction
 *
 * @param thread_stop atomic boolean that signals this thread to stop
 * @param fileWatcher_stop atomic boolean that signals the filesystem watcher to stop
 * @param eventQueue queue of events generated by the filesystem watcher
 * @param server_ip ip address of the server to connect to
 * @param server_port port of the server to connect to
 * @param username username of the current user
 * @param password password of the current user
 *
 * @author Michele Crepaldi
 */
void communicate(std::atomic<bool> &thread_stop, std::atomic<bool> &fileWatcher_stop, TSCircular_vector<Event> &eventQueue, const std::string &server_ip,
                 const std::string &server_port, const std::string &username, const std::string &password) {

    //initialize socket address
    struct sockaddr_in server_address = Socket::composeAddress(server_ip, server_port);

    try{
        //initialize socket
        Socket client_socket; //may throw an exception!
        //initialize protocol manager
        ProtocolManager pm(client_socket, db, MAX_RESPONSE_WAITING, VERSION, MAX_SERVER_ERROR_RETRIES);

        //for select
        fd_set read_fds;
        fd_set write_fds;
        //variable used to count number of consecutive connection re-tries (connection error)
        int tries = 0, authTries = 0;

        // this thread will loop until it will be told to stop
        while (!thread_stop.load()) {
            try {

                //wait until there is at least one event in the event queue (blocking wait)
                if(!eventQueue.waitForCondition(thread_stop)) //returns true if an event can be popped from the queue, false if thread_stop is true, otherwise it stays blocked
                    return; //if false then we exited the condition for the thread_stop being true so we want to close the program

                //connect with server
                client_socket.connect(&server_address, sizeof(server_address));
                //if the connection is successful then reset tries variable
                tries = 0;

                //authenticate user
                pm.authenticate(username, password); //TODO add MAC
                //if the authentication is successful reset tries
                authTries = 0;

                //if there was an exception then resend all unacknowledged messages
                pm.recoverFromError();

                //some variables for the loop and select
                int timeWaited = 0;
                bool loop = true;

                //after connection and authentication then iteratively do this;
                //if any connection error occurs then redo connection and authentication
                while (loop && !thread_stop.load()) { //if thread_stop is true then i want to exit

                    //build fd sets
                    FD_ZERO(&read_fds);
                    FD_SET(client_socket.getSockfd(), &read_fds);

                    FD_ZERO(&write_fds);
                    // there is something to send, set up write_fd for server socket
                    if (pm.canSend() && eventQueue.canGet())
                        FD_SET(client_socket.getSockfd(), &write_fds);

                    int maxfd = client_socket.getSockfd();
                    struct timeval tv{};
                    tv.tv_sec = SELECT_TIMEOUT;

                    int activity = select(maxfd + 1, &read_fds, &write_fds, nullptr, &tv);

                    if (thread_stop.load()) { //if thread_stop atomic boolean is true then close connection and return
                        //quit connection
                        pm.quit();
                        //close connection
                        client_socket.closeConnection();
                        return;
                    }

                    switch (activity) {
                        case -1:
                            //I should never get here
                            std::cerr << "Select error" << std::endl;

                            //connection will be closed automatically by the socket destructor

                            //terminate filesystem watcher and close program
                            fileWatcher_stop.store(true);
                            return;
                            break;

                        case 0:
                            timeWaited += SELECT_TIMEOUT;

                            if(timeWaited >= TIMEOUT)   //if the time already waited is greater than TIMEOUT
                                loop = false;           //then exit inner loop --> this will cause the connection to be closed

                            break;

                        default:
                            //reset timeout
                            timeWaited = 0;

                            //if I have something to write and I can write
                            if (FD_ISSET(client_socket.getSockfd(), &write_fds)) {
                                // if an event has occured then assign it to the current event to be handled
                                pm.send(eventQueue.front()); //if an exception occurs now I won't pop the queue so next time I'll get the same element

                                //in case everything went smoothly then pop event from event queue
                                eventQueue.pop();
                            }

                            //if I have something to read
                            if (FD_ISSET(client_socket.getSockfd(), &read_fds)) {
                                pm.receive();
                            }
                    }

                }

                //quit connection
                pm.quit();

                //close connection
                client_socket.closeConnection();
            }
            catch (SocketException &e) {
                //error in connection; retry later; wait for x seconds

                //try only for a limited number of times
                if(tries > MAX_CONNECTION_RETRIES){
                    //maximum number of re-tries exceeded -> terminate filesystem watcher and close program
                    fileWatcher_stop.store(true);
                    return;
                }

                tries++;
                std::cout << "Connection error. Retry (" << tries << ") in " << SECONDS_BETWEEN_RECONNECTIONS << " seconds." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(SECONDS_BETWEEN_RECONNECTIONS));
            }
            catch (ProtocolManagerException &e) {
                switch(e.getErrorCode()){
                    case -1: //unsupported message type error
                    case 0: //unknown server error
                    case 401: //authentication error
                        std::cerr << e.what() << std::endl;

                        //connection will be closed automatically by the socket destructor

                        //terminate filesystem watcher and close program
                        fileWatcher_stop.store(true);
                        return;

                    case 505: //version not supported error
                        std::cerr << e.what() << "; try with version" << e.getData() << std::endl;

                        //connection will be closed automatically by the socket destructor

                        //terminate filesystem watcher and close program
                        fileWatcher_stop.store(true);
                        return;

                    case 500: //internal server error

                        if(e.getData() == 0){ //internal server error while NOT authenticated
                            //retry the authentication x times

                            //close connection
                            client_socket.closeConnection();

                            if(authTries > MAX_AUTH_ERROR_RETRIES){
                                //terminate filesystem watcher and close program
                                fileWatcher_stop.store(true);
                                return;
                            }

                            authTries++;
                        }
                        else{ //internal server error while authenticated (in receive)
                            //retries are already exceeded (the protocol manager handles retries)

                            //connection will be closed automatically by the socket destructor

                            //terminate filesystem watcher and close program
                            fileWatcher_stop.store(true);
                            return;
                        }

                    default: //for unrecognised exception codes
                        std::cerr << "Unrecognised exception code" << std::endl;

                        //connection will be closed automatically by the socket destructor

                        //terminate filesystem watcher and close program
                        fileWatcher_stop.store(true);
                        return;
                }
            }
            catch (DatabaseException &e) {
                //in case of a database exception the only thing we can do is to close the program
                std::cerr << e.what() << std::endl;

                //connection will be closed automatically by the socket destructor

                //terminate filesystem watcher and close program
                fileWatcher_stop.store(true);
                return;
            }
        }
    }
    catch (SocketException &e) {
        //we are here if the socket class couldn't create a sokcet
        std::cerr << e.what() << std::endl;

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);
        return;
    }
    catch (...) {
        //any uncaught exception will terminate the program
        std::cerr << "uncaught exception" << std::endl;

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);
        return;
    }
}