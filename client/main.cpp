//
// Created by michele on 25/07/2020.
//

#include <chrono>
#include <string>
#include <iostream>
#include <atomic>
#include <regex>
#include "FileSystemWatcher.h"
#include "Event.h"
#include "../myLibraries/Circular_vector.h"
#include "Thread_guard.h"
#include "../myLibraries/Socket.h"
#include "messages.pb.h"
#include "ProtocolManager.h"
#include "../myLibraries/Message.h"
#include "ArgumentsManager.h"
#include "Config.h"

//TODO check

#define VERSION 1

#define SOCKET_TYPE SocketType::TLS
#define CONFIG_FILE_PATH "../config.txt"

void communicate(std::atomic<bool> &, std::atomic<bool> &, TS_Circular_vector<Event> &, const std::string &, int, const std::string &, const std::string &);

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

    try {
        client::ArgumentsManager inputArgs = client::ArgumentsManager(argc, argv);  //main input arguments

        //get the configuration
        client::Config::setPath(CONFIG_FILE_PATH);
        auto config = client::Config::getInstance();
        //get the database instance and open the database (and if not previously there create also the needed table)
        client::Database::setPath(config->getDatabasePath());
        auto db = client::Database::getInstance();

        if(inputArgs.isRetrSet()){
            //specfy the TLS certificates for the socket
            Socket::specifyCertificates(config->getCAFilePath());
            //create the socket
            Socket client_socket{SOCKET_TYPE};

            //connect to the server through the socket
            client_socket.connect(inputArgs.getServerIp(), stoi(inputArgs.getSeverPort()));

            //create the protocol manager instance
            client::ProtocolManager pm(client_socket, VERSION);

            //authenticate the client to the server
            pm.authenticate(inputArgs.getUsername(), inputArgs.getPassword(), client_socket.getMAC());

            //send the RETR message and get all the data from server
            if(inputArgs.isMacSet()){
                Message::print(std::cout, "INFO", "Will retrieve all your files corresponding to mac: " + inputArgs.getMac(), "destination folder: " + inputArgs.getDestFolder());

                //send RETR message with the set mac and get all data from server and save it in destFolder
                pm.retrieveFiles(inputArgs.getMac(), false, inputArgs.getDestFolder());
            }
            else if(inputArgs.isAllSet()){
                Message::print(std::cout, "INFO", "Will retrieve all your files", "destination folder: " + inputArgs.getDestFolder());

                //send RETR message with all set and get all data from server and save it in destFolder
                pm.retrieveFiles("", true, inputArgs.getDestFolder());
            }
            else{
                std::string thisSocketMac = client_socket.getMAC();
                Message::print(std::cout, "INFO", "Will retrieve all your files corresponding to mac: " + thisSocketMac, "destination folder: " + inputArgs.getDestFolder());

                //send RETR message with this machine's mac address and get all data from server and save it in destFolder
                pm.retrieveFiles(thisSocketMac, false, inputArgs.getDestFolder());
            }
        }

        if(!inputArgs.isStartSet())   //if the --start option was not there just close the program
            return 0;

        // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
        FileSystemWatcher fw{config->getPathToWatch(), std::chrono::milliseconds(config->getMillisFilesystemWatcher())};

        // create a circular vector instance that will contain all the events happened
        TS_Circular_vector<Event> eventQueue(config->getEventQueueSize());

        //initialize some atomic boolean to make the different threads stop
        std::atomic<bool> communicationThread_stop = false, fileWatcher_stop = false;

        // initialize the communication thread (thread that will communicate with the server)
        std::thread communication_thread(communicate, std::ref(communicationThread_stop), std::ref(fileWatcher_stop), std::ref(eventQueue), inputArgs.getServerIp(), stoi(inputArgs.getSeverPort()), inputArgs.getUsername(), inputArgs.getPassword());

        // use thread guard to signal to the communication thread to stop and wait for it in case we exit the main
        client::Thread_guard tg_communication(communication_thread, communicationThread_stop);

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
    catch (client::ArgumentsManagerException &e) {
        switch(e.getCode()){
            case client::ArgumentsManagerError::help:
                return 0;
            case client::ArgumentsManagerError::numberOfArguments:
            case client::ArgumentsManagerError::option:
            case client::ArgumentsManagerError::optArgument:
            default:
                Message::print(std::cerr, "ERROR", "Input Exception", e.what());

                return 1;
        }
    }
    catch (SocketException &e) {
        //TODO handle socket exceptions
        return 1;
    }
    catch (client::ProtocolManagerException &e) {
        //TODO handle protocol manager exceptions
        return 1;
    }
    catch (client::DatabaseException &e) {
        switch(e.getCode()){
            case client::DatabaseError::path:   //no path was provided to the Database class -> (fatal) terminate client
            case client::DatabaseError::create: //could not create the table in the database -> (fatal) terminate client
            case client::DatabaseError::open:   //could not open the database -> (fatal) terminate client
            case client::DatabaseError::prepare:    //could not prepare a SQL statement -> (fatal) terminate client
            case client::DatabaseError::finalize:   //could not finalize SQL statement -> (fatal) terminate client
            case client::DatabaseError::insert: //could not insert into the database -> (fatal) terminate client (not used here)
            case client::DatabaseError::read:   //could not read from the database -> (fatal) terminate client (not used here)
            case client::DatabaseError::update: //could not update into the database -> (fatal) terminate client (not used here)
            case client::DatabaseError::remove: //could not remove from the database -> (fatal) terminate client (not used here)
            default:
                //Fatal error -> close the server
                Message::print(std::cerr, "ERROR", "Database Exception", e.what());
                return 1;
        }
    }
    catch (client::ConfigException &e) {
        switch(e.getCode()){
            case client::ConfigError::justCreated:  //if the config file was not there and it has been created
            case client::ConfigError::pathToWatch:  //if the configured path to watch does not exist ask to modify it and return
                Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

            case client::ConfigError::open: //if there were some errors in opening the configuration file return
            default:
                Message::print(std::cerr, "ERROR", "Config Exception", e.what());
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
void communicate(std::atomic<bool> &thread_stop, std::atomic<bool> &fileWatcher_stop, TS_Circular_vector<Event> &eventQueue, const std::string &server_ip,
                 int server_port, const std::string &username, const std::string &password) {

    try {

        //get the configuration
        client::Config::setPath(CONFIG_FILE_PATH);
        auto config = client::Config::getInstance();

        Socket::specifyCertificates(config->getCAFilePath());

        //for select
        fd_set read_fds;
        fd_set write_fds;
        //variable used to count number of consecutive connection re-tries (connection error)
        int tries = 0;
        bool authenticated = false;

        // this thread will loop until it will be told to stop
        while (!thread_stop.load()) {
            //initialize socket
            Socket client_socket(SOCKET_TYPE);

            //initialize protocol manager
            client::ProtocolManager pm(client_socket, VERSION);

            try {

                //wait until there is at least one event in the event queue (blocking wait)
                if (!eventQueue.waitForCondition(thread_stop)) //returns true if an event can be popped from the queue, false if thread_stop is true, otherwise it stays blocked
                    return; //if false then we exited the condition for the thread_stop being true so we want to close the program

                Message::print(std::cout, "INFO", "Changes detected", "Connecting to server..");
                //connect with server
                client_socket.connect(server_ip, server_port);
                //if the connection is successful then reset tries variable
                tries = 0;

                //authenticate user
                pm.authenticate(username, password, client_socket.getMAC());
                authenticated = true;

                //if there was an exception then resend all unacknowledged messages
                pm.recoverFromError();

                //some variables for the loop and select
                unsigned int timeWaited = 0;
                bool loop = true;

                //after connection and authentication then iteratively do this;
                //if any connection error occurs then redo connection and authentication
                while (loop && !thread_stop.load()) { //if thread_stop is true then i want to exit

                    //build fd sets
                    FD_ZERO(&read_fds);
                    FD_SET(0, &read_fds);
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
                        //connection will be automatically closed (by socket destructor)
                        return;
                    }

                    switch (activity) {
                        case -1:
                            //I should never get here
                            Message::print(std::cerr, "ERROR", "Select error", "case -1");

                            //connection will be closed automatically by the socket destructor

                            //terminate filesystem watcher and close program
                            fileWatcher_stop.store(true);
                            return;

                        case 0:
                            if(pm.isWaiting())  //if the protocol manager is waiting for server responses just go on
                                break;

                            //if the protocol manager is not waiting for any server response then update the timeWaited variable
                            //to then close the connection if after TimeoutSeconds seconds no event happens
                            timeWaited += config->getSelectTimeoutSeconds();

                            if (timeWaited >= config->getTimeoutSeconds()) {  //if the time already waited is greater than TIMEOUT
                                loop = false;           //then exit inner loop --> this will cause the connection to be closed
                                Message::print(std::cout, "INFO", "No changes detected", "Disconnecting from server..");
                            }

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

                            //if I have something to read
                            if (FD_ISSET(0, &read_fds)) {
                                std::string command;
                                std::cin >> command;
                                if(command == "exit"){
                                    //client_socket.closeConnection();
                                    fileWatcher_stop.store(true);
                                    return;
                                }

                            }
                    }

                }

                //close connection
                client_socket.closeConnection();
            }
            catch (SocketException &e) {
                switch (e.getCode()) {
                    case SocketError::closed:    //socket was closed by server
                    //case socketError::create:   //error in socket create
                    case SocketError::read:     //error in socket read
                    case SocketError::write:    //error in socket write
                    case SocketError::connect:  //error in socket connect

                        if(tries == 0)
                            Message::print(std::cout, "INFO", "Connection was closed by the server", "will reconnect if needed");

                        if(!pm.canSend() || !eventQueue.canGet())
                            break;

                        //re-try only for a limited number of times
                        if (tries < config->getMaxConnectionRetries()) {
                            //error in connection; retry later; wait for x seconds
                            tries++;
                            std::stringstream tmp;
                            tmp << "Retry (" << tries << ") in " << config->getSecondsBetweenReconnections() << " seconds.";
                            Message::print(std::cerr, "WARNING", "Connection error", tmp.str());
                            std::this_thread::sleep_for(std::chrono::seconds(config->getSecondsBetweenReconnections()));
                            break;
                        }

                        //maximum number of re-tries exceeded -> terminate program
                        Message::print(std::cerr, "ERROR", "Cannot establish connection");

                    case SocketError::getMac: //error retrieving the MAC
                    default:
                        //terminate filesystem watcher and close program
                        fileWatcher_stop.store(true);
                        return;
                }
            }
            catch (client::ProtocolManagerException &e) {
                switch (e.getErrorCode()) {
                    case client::ProtocolManagerError::auth:
                    case client::ProtocolManagerError::internal:
                    case client::ProtocolManagerError::serverMessage:
                    case client::ProtocolManagerError::version:
                        Message::print(std::cerr, "ERROR", "ProtocolManager Exception", e.what());

                        //connection will be closed automatically by the socket destructor

                        //terminate filesystem watcher and close program
                        fileWatcher_stop.store(true);
                        return;

                    case client::ProtocolManagerError::unexpected:
                    case client::ProtocolManagerError::unexpectedCode:
                        if(!authenticated){  //if I am not authenticated any unexpected message means error
                            Message::print(std::cerr, "ERROR", "ProtocolManager Exception", e.what());

                            //connection will be closed automatically by the socket destructor

                            //terminate filesystem watcher and close program
                            fileWatcher_stop.store(true);
                            return;
                        }

                        //otherwise
                    case client::ProtocolManagerError::client: //client message error
                        //TODO go on -> skip the current message (Should be done.. check..)
                        Message::print(std::cerr, "WARNING", "Message error", "Message will be skipped");
                        break;

                    default: //for unrecognised exception codes
                        Message::print(std::cerr, "ERROR", "ProtocolManager Exception", "Unrecognised exception code");

                        //connection will be closed automatically by the socket destructor

                        //terminate filesystem watcher and close program
                        fileWatcher_stop.store(true);
                        return;
                }
            }
            catch (client::DatabaseException &e) {
                throw;  //rethrow the exception //TODO check this
            }
        }
    }
    catch (SocketException &e) {
        //we are here if the socket class couldn't create a socket
        Message::print(std::cerr, "ERROR", "Socket Exception", e.what());

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);
        return;
    }
    catch (client::DatabaseException &e){
        switch(e.getCode()){
            case client::DatabaseError::path:   //no path was provided to the Database class -> (fatal) terminate client
            case client::DatabaseError::create: //could not create the table in the database -> (fatal) terminate client
            case client::DatabaseError::open:   //could not open the database -> (fatal) terminate client
            case client::DatabaseError::prepare:    //could not prepare a SQL statement -> (fatal) terminate client
            case client::DatabaseError::finalize:   //could not finalize SQL statement -> (fatal) terminate client
            case client::DatabaseError::insert: //could not insert into the database -> (fatal) terminate client (not used here)
            case client::DatabaseError::read:   //could not read from the database -> (fatal) terminate client (not used here)
            case client::DatabaseError::update: //could not update into the database -> (fatal) terminate client (not used here)
            case client::DatabaseError::remove: //could not remove from the database -> (fatal) terminate client (not used here)
            default:
                //Fatal error -> close the server
                Message::print(std::cerr, "ERROR", "Database Exception", e.what());

                //terminate filesystem watcher and close program
                fileWatcher_stop.store(true);
                return;
        }
    }
    catch (client::ConfigException &e) {
        switch(e.getCode()){
            case client::ConfigError::justCreated:  //if the config file was not there and it has been created
            case client::ConfigError::pathToWatch:  //if the configured path to watch was not specified or it does not exist (or it is not a directory) ask to modify it and return
                Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

            case client::ConfigError::open: //if there were some errors in opening the configuration file return
            default:
                Message::print(std::cerr, "ERROR", "Config Exception", e.what());
        }

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);
        return;
    }
    catch (...) {
        //any uncaught exception will terminate the program
        Message::print(std::cerr, "ERROR", "uncaught exception");

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);
        return;
    }
}