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

#define VERSION 1
#define CONFIG_FILE_PATH "./config.txt"

void communicate(std::atomic<bool> &, std::atomic<bool> &, TSCircular_vector<Event> &, const std::string &, int, const std::string &, const std::string &);

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

    if (argc != 5) {
        std::cerr << "Error: format is [" << argv[0] << "] [server ip] [server port] [username] [password]" << std::endl;
        exit(1);
    }

    try {
        //get the configuration
        auto config = client::Config::getInstance(std::string(CONFIG_FILE_PATH));
        //get the database instance and open the database (and if not previously there create also the needed table)
        auto db = client::Database::getInstance(config->getDatabasePath());

        // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
        FileSystemWatcher fw{config->getPathToWatch(), std::chrono::milliseconds(config->getMillisFilesystemWatcher())};

        // create a circular vector instance that will contain all the events happened
        TSCircular_vector<Event> eventQueue(config->getEventQueueSize());

        //extract the arguments
        std::string serverIP = argv[1];
        std::string serverPort = argv[2];
        std::string username = argv[3];
        std::string password = argv[4];

        //initialize some atomic boolean to make the different threads stop
        std::atomic<bool> communicationThread_stop = false, fileWatcher_stop = false;

        // initialize the communication thread (thread that will communicate with the server)
        std::thread communication_thread(communicate, std::ref(communicationThread_stop), std::ref(fileWatcher_stop), std::ref(eventQueue), serverIP, stoi(serverPort), username, password);

        // use thread guard to signal to the communication thread to stop and wait for it in case we exit the main
        Thread_guard tg_communication(communication_thread, communicationThread_stop);

        //make the filesystem watcher retrieve previously saved data from db
        fw.recoverFromDB(db.get(), [&eventQueue](Directory_entry &element, FileSystemStatus status) {
            //push event into event queue
            eventQueue.push(std::move(Event(element, status)));
        });

        // Start monitoring a folder for changes and (in case of changes) run a user provided lambda function
        fw.start([&eventQueue](Directory_entry &element, FileSystemStatus status) -> bool {
            //it returns true if the object was successfully pushed, false otherwise; in any case it returns immediately with no waiting.
            //This is done is such way as to not be blocked waiting for the queue to be not full.
            //So elements not pushed successfully won't be added to the already watched ones and this process will be repeated after some time
            return eventQueue.tryPush(std::move(Event(element, status)));
        }, fileWatcher_stop);

    }
    catch (client::DatabaseException &e) {
        //in case of database exceptions show message and return
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (client::ConfigException &e) {
        if (e.getCode() == client::configError::fileCreated){   //if the config file did not exist create it, ask to modify it and return
            std::cout << e.what() << std::endl;
            std::cout << "Configuration file now contains default values; modify it and especially set a value for 'path_to_watch' before restarting the application." << std::endl;
            std::cout << "You can find the file here: " << CONFIG_FILE_PATH << std::endl;
        }
        else if(e.getCode() == client::configError::open){  //if there were some errors in opening the configuration file return
            std::cerr << e.what() << std::endl;
        }

        return 1;
    }

    return 0;
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
                 int server_port, const std::string &username, const std::string &password) {

    try {
        //initialize socket
        Socket client_socket(socketType::TCP);

        //get the configuration
        auto config = client::Config::getInstance(std::string(CONFIG_FILE_PATH));

        //initialize protocol manager
        client::ProtocolManager pm(client_socket, config->getMaxResponseWaiting(), VERSION, config->getMaxServerErrorRetries(), config->getPathToWatch());

        //for select
        fd_set read_fds;
        fd_set write_fds;
        //variable used to count number of consecutive connection re-tries (connection error)
        int tries = 0, authTries = 0;

        // this thread will loop until it will be told to stop
        while (!thread_stop.load()) {
            try {

                //wait until there is at least one event in the event queue (blocking wait)
                if (!eventQueue.waitForCondition(
                        thread_stop)) //returns true if an event can be popped from the queue, false if thread_stop is true, otherwise it stays blocked
                    return; //if false then we exited the condition for the thread_stop being true so we want to close the program

                //connect with server
                client_socket.connect(server_ip, server_port);
                //if the connection is successful then reset tries variable
                tries = 0;

                //authenticate user
                pm.authenticate(username, password, client_socket.getMAC());
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
                    tv.tv_sec = config->getSelectTimeoutSeconds();

                    int activity = select(maxfd + 1, &read_fds, &write_fds, nullptr, &tv);

                    //TODO check if it is still needed
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

                        case 0:
                            timeWaited += config->getSelectTimeoutSeconds();

                            if (timeWaited >= config->getTimeoutSeconds())   //if the time already waited is greater than TIMEOUT
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
                switch (e.getCode()) {
                    //case socketError::create:   //error in socket create
                    case socketError::read:     //error in socket read
                    case socketError::write:    //error in socket write
                    case socketError::connect:  //error in socket connect

                        //re-try only for a limited number of times
                        if (tries < config->getMaxConnectionRetries()) {
                            //error in connection; retry later; wait for x seconds
                            tries++;
                            std::cout << "Connection error. Retry (" << tries << ") in "
                                      << config->getSecondsBetweenReconnections() << " seconds." << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(config->getSecondsBetweenReconnections()));
                            break;
                        }

                        //maximum number of re-tries exceeded -> terminate program
                        std::cerr << "Cannot establish connection." << std::endl;

                    case socketError::getMac: //error retrieving the MAC
                    default:
                        //terminate filesystem watcher and close program
                        fileWatcher_stop.store(true);
                        return;
                }
            }
            catch (client::ProtocolManagerException &e) {
                switch (e.getErrorCode()) {
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

                        if (e.getData() == 0) { //internal server error while NOT authenticated
                            //retry the authentication x times

                            //close connection
                            client_socket.closeConnection();

                            if (authTries > config->getMaxAuthErrorRetries()) {
                                //terminate filesystem watcher and close program
                                fileWatcher_stop.store(true);
                                return;
                            }

                            authTries++;
                        } else { //internal server error while authenticated (in receive)
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
            catch (client::DatabaseException &e) {
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
        //we are here if the socket class couldn't create a socket
        std::cerr << e.what() << std::endl;

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);
        return;
    }
    catch (client::DatabaseException &e){
        //we are here if the Database class couldn't open the database
        std::cerr << e.what() << std::endl;

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);
        return;
    }
    catch (client::ConfigException &e) {
        if (e.getCode() == client::configError::fileCreated){   //if the config file did not exist create it, ask to modify it and return
            std::cout << e.what() << std::endl;
            std::cout << "Configuration file now contains default values; modify it and especially set a value for 'path_to_watch' before restarting the application." << std::endl;
            std::cout << "You can find the file here: " << CONFIG_FILE_PATH << std::endl;
        }
        else if(e.getCode() == client::configError::open){  //if there were some errors in opening the configuration file return
            std::cerr << e.what() << std::endl;
        }

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