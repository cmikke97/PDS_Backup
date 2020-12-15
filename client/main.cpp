//
// Created by Michele Crepaldi s269551 on 25/07/2020
// Finished on 22/11/2020
// Last checked on 22/11/2020
//


#include <messages.pb.h>

#include <chrono>
#include <string>
#include <iostream>
#include <atomic>

#include "../myLibraries/Socket.h"
#include "../myLibraries/Circular_vector.h"
#include "../myLibraries/Message.h"
#include "../myLibraries/Hash.h"
#include "../myLibraries/RandomNumberGenerator.h"

#include "FileSystemWatcher.h"
#include "Event.h"
#include "Thread_guard.h"
#include "ProtocolManager.h"
#include "ArgumentsManager.h"
#include "Config.h"


#define VERSION 1

#define SOCKET_TYPE SocketType::TLS
#define CONFIG_FILE_PATH "../config.txt"

using namespace client;

//function to handle the communication with the server
void communicate(std::atomic<bool> &, std::atomic<bool> &, TS_Circular_vector<Event> &, const std::string &, int,
                 const std::string &, const std::string &, bool);

/**
 * main function
 *
 * @param argc number of main arguments
 * @param argv main arguments
 * @return exit value
 *
 * @author Michele Crepaldi s269551
 */
int main(int argc, char **argv) {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    try {
        //get options and option arguments from the main arguments

        ArgumentsManager inputArgs{argc, argv}; //main input arguments

        Config::setPath(CONFIG_FILE_PATH);  //set the configuration file path
        auto config = Config::getInstance();    //config instance

        Database::setPath(config->getDatabasePath());         //set the database path
        auto db = Database::getInstance();          //server database instance

        if(inputArgs.isRetrSet()){  //if retrieve option is set
            //retrieve the user's data and store it in the specified folder

            //specify the CA certificate path to be used by the TLS socket
            Socket::specifyCertificates(config->getCAFilePath());

            Socket client_socket{SOCKET_TYPE};  //client socket

            //connect to the server
            client_socket.connect(inputArgs.getServerIp(), stoi(inputArgs.getSeverPort()));

            //queue of messages sent and waiting for a server response
            Circular_vector<Event> waitingForResponse{1};
            ProtocolManager pm(client_socket, waitingForResponse, VERSION); //protocol manager instance

            //authenticate the client to the server (using username, password and mac address)
            pm.authenticate(inputArgs.getUsername(), inputArgs.getPassword(), client_socket.getMAC());

            //send the RETR message and get all the data from server

            if(inputArgs.isMacSet()){   //if mac option is set
                //retrieve all data corresponding to the (username,mac) pair

                Message::print(std::cout, "INFO", "Will retrieve all your files corresponding to mac: "
                                + inputArgs.getMac(), "destination folder: " + inputArgs.getDestFolder());

                //send RETR message with the specified mac (with all boolean to false),
                //get all data from server and save it in destFolder
                pm.retrieveFiles(inputArgs.getMac(), false, inputArgs.getDestFolder());
            }
            else if(inputArgs.isAllSet()){  //else if all option is set
                //retrieve all the user's data

                Message::print(std::cout, "INFO", "Will retrieve all your files",
                               "destination folder: " + inputArgs.getDestFolder());

                //send RETR message with the with all boolean to true (and no specified mac),
                //get all data from server and save it in destFolder
                pm.retrieveFiles("", true, inputArgs.getDestFolder());
            }
            else{   //otherwise
                //retrieve all the user's data corresponding to the current mac address

                std::string thisSocketMac = client_socket.getMAC(); //get mac address from the current socket

                Message::print(std::cout, "INFO", "Will retrieve all your files corresponding to mac: "
                                + thisSocketMac, "destination folder: " + inputArgs.getDestFolder());

                //send RETR message with the current mac (with all boolean to false),
                //get all data from server and save it in destFolder
                pm.retrieveFiles(thisSocketMac, false, inputArgs.getDestFolder());
            }
        }

        if(!inputArgs.isStartSet())   //if the start option was not set just close the program
            return 0;

        Message::print(std::cout, "INFO", "Starting service..",
                       "Watching " + config->getPathToWatch() + " for changes");

        //FileSystemWatcher instance that will check the current folder for changes every X milliseconds
        FileSystemWatcher fw{config->getPathToWatch(), std::chrono::milliseconds(config->getMillisFilesystemWatcher())};

        //event queue that will contain all the events happened on the watched path
        TS_Circular_vector<Event> eventQueue(config->getEventQueueSize());

        //atomic boolean used to force the communication thread to stop
        std::atomic<bool> communicate_stop = false;
        //atomic boolean used to force the file system watcher thread to stop
        std::atomic<bool> fileWatcher_stop = false;

        //create communication thread and start it
        std::thread communication_thread(communicate, std::ref(communicate_stop),
                                         std::ref(fileWatcher_stop), std::ref(eventQueue),
                                         std::ref(inputArgs.getServerIp()), stoi(inputArgs.getSeverPort()),
                                         std::ref(inputArgs.getUsername()), std::ref(inputArgs.getPassword()),
                                         inputArgs.isPersistSet());

        //thread guard used to stop the communication thread when the file system watcher returns
        Thread_guard tg_communication(communication_thread, communicate_stop);

        //make the filesystem watcher retrieve previously saved data from db
        fw.recoverFromDB(db.get(), [&eventQueue, &fileWatcher_stop](Directory_entry &element, FileSystemStatus status) -> bool {
            //push event into event queue (to check the server has copies of all files we have in the db)
            return eventQueue.push(std::move(Event(element, status)), fileWatcher_stop);
        });

        //start monitoring the path to watch for changes and (in case of changes) run the provided function
        fw.start([&eventQueue](Directory_entry &element, FileSystemStatus status) -> bool {
            //try to push the event inside the event queue, immediately returning if the queue is full
            //non-pushed elements will be re-detected after some time and this process will be repeated
            return eventQueue.tryPush(std::move(Event(element, status)));
        }, fileWatcher_stop);

    }
    catch (ArgumentsManagerException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case ArgumentsManagerError::help:
                return 0;
            case ArgumentsManagerError::numberOfArguments:
            case ArgumentsManagerError::option:
            case ArgumentsManagerError::optArgument:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "ArgumentsManager Exception", e.what());

                return 1;
        }
    }
    catch (ConfigException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case ConfigError::justCreated:
            case ConfigError::pathToWatch:
                Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

            case ConfigError::path:
            case ConfigError::open:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Config Exception", e.what());

                return 1;
        }
    }
    catch (DatabaseException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case DatabaseError::path:
            case DatabaseError::create:
            case DatabaseError::open:
            case DatabaseError::prepare:
            case DatabaseError::finalize:
            case DatabaseError::insert:
            case DatabaseError::read:
            case DatabaseError::update:
            case DatabaseError::remove:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Database Exception", e.what());

                return 1;
        }
    }
    catch (ProtocolManagerException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case ProtocolManagerError::auth:
            case ProtocolManagerError::internal:
            case ProtocolManagerError::client:
            case ProtocolManagerError::serverMessage:
            case ProtocolManagerError::version:
            case ProtocolManagerError::unexpected:
            case ProtocolManagerError::unexpectedCode:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "ProtocolManager Exception", e.what());

                return 1;
        }
    }
    catch (SocketException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case SocketError::create:
            case SocketError::accept:
            case SocketError::bind:
            case SocketError::read:
            case SocketError::write:
            case SocketError::connect:
            case SocketError::getMac:
            case SocketError::getIp:
            case SocketError::closed:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Socket Exception", e.what());

                return 1;
        }
    }
    catch (HashException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case HashError::init:
            case HashError::update:
            case HashError::finalize:
            case HashError::set:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Hash Exception", e.what());

                return 1;
        }
    }
    catch (RngException &e) {
        //switch on the exception code
        switch(e.getCode()) {
            case RngError::init:
            case RngError::generate:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Rng Exception", e.what());

                return 1;
        }
    }
    catch (std::exception &e) {
        //print a message and exit

        Message::print(std::cerr, "ERROR", "Generic exception", e.what());

        return 1;
    }
    catch (...) {
        //print a message and exit

        Message::print(std::cerr, "ERROR", "uncaught exception");

        return 1;
    }

    return 1;
}

/**
 * communicate function.
 *  function representing a communication thread. Used to handle the client - server interaction
 *
 * @param communicate_stop atomic boolean to stop this communication thread
 * @param fileWatcher_stop atomic boolean to stop the fileWatcher (main) thread
 * @param eventQueue event queue filled by the filesystem watcher
 * @param server_ip ip address of the server to connect to
 * @param server_port port of the server to connect to
 * @param username username of the current user
 * @param password password of the current user
 *
 * @author Michele Crepaldi
 */
void communicate(std::atomic<bool> &communicate_stop, std::atomic<bool> &fileWatcher_stop,
                 TS_Circular_vector<Event> &eventQueue, const std::string &server_ip,
                 int server_port, const std::string &username, const std::string &password, bool persist) {

    int connectionCounter = 0;

    try {
        auto config = Config::getInstance();    //config instance

        //specify the CA certificate path to be used by the TLS socket
        Socket::specifyCertificates(config->getCAFilePath());

        //queue of messages sent and waiting for a server response
        Circular_vector<Event> waitingForResponse{config->getMaxResponseWaiting()};

        fd_set read_fds;    //fd read set for select
        fd_set write_fds;   //fd write set for select

        int tries = 0;  //number of consecutive connection retries

        //loop until we are told to stop
        while (!communicate_stop.load()) {
            Socket client_socket(SOCKET_TYPE);  //client socket

            //protocol manager for this connection
            ProtocolManager pm(client_socket, waitingForResponse, VERSION);

            try {

                //if the waiting messages queue is empty just passively wait on new events
                if(!pm.isWaiting()) {
                    //wait until there is at least one event in the event queue (blocking wait).
                    //It returns true if an event can be popped from the queue, false if thread_stop is true,
                    //otherwise it stays blocked
                    if (!eventQueue.waitForCondition(communicate_stop))
                        //if false it means that we returned because thread_stop became true, so close the program
                        return;
                }

                Message::print(std::cout, "INFO", "Changes detected", "Connecting to server..");

                //connect with server
                client_socket.connect(server_ip, server_port);

                tries = 0; //reset consecutive connection retries
                connectionCounter++; //update the connection counter

                Message::print(std::cout, "INFO", "Connection #"
                                + std::to_string(connectionCounter) + " established");

                //authenticate user
                pm.authenticate(username, password, client_socket.getMAC());

                //resend all unacknowledged messages (in case we just recovered from a connection error)
                pm.recoverFromError();

                //total time waited without detecting changed in the folder and without receiving messages from server
                unsigned int timeWaited = 0;
                bool loop = true;   //loop condition

                //loop until we are told to stop or loop condition becomes false
                while (loop && !communicate_stop.load()) {
                    //build fd sets

                    FD_ZERO(&read_fds);
                    FD_SET(client_socket.getSockfd(), &read_fds);
                    FD_ZERO(&write_fds);

                    //if we can send messages and there is something to send
                    if (pm.canSend() && eventQueue.canGet())
                        //set up write_fd for socket
                        FD_SET(client_socket.getSockfd(), &write_fds);

                    int maxfd = client_socket.getSockfd();  //select max fd
                    struct timeval tv{};    //timeval struct for select
                    tv.tv_sec = config->getSelectTimeoutSeconds();  //set timeval for the select function

                    //select on read and write socket

                    int activity = select(maxfd + 1, &read_fds, &write_fds, nullptr, &tv);

                    switch (activity) {
                        case -1:
                            //I should never get here
                            Message::print(std::cerr, "ERROR", "Select error");

                            //connection will be closed automatically by the socket destructor

                            //terminate filesystem watcher and close program
                            fileWatcher_stop.store(true);
                            return;

                        case 0:
                            if(pm.isWaiting())  //if the protocol manager is waiting for server responses
                                //just go on
                                break;

                            //otherwise

                            timeWaited += config->getSelectTimeoutSeconds();    //update the timeWaited variable

                            //if the time already waited is greater than TimeoutSeconds
                            if (timeWaited >= config->getTimeoutSeconds()) {
                                Message::print(std::cout, "INFO", "No changes detected",
                                               "Disconnecting from server..");

                                //exit inner loop --> this will cause the connection to be closed
                                loop = false;
                            }

                            break;

                        default:
                            //reset timeout
                            timeWaited = 0;

                            //if I have something to write and I can write
                            if (FD_ISSET(client_socket.getSockfd(), &write_fds)) {
                                //send a message related to the next event in the event queue without removing it
                                //from the queue; doing so in case of connection error I will not lose the event
                                //and I will retry with the same one
                                if(pm.send(eventQueue.front())) //if the event was sent (or was of an unsupported type)
                                    //pop the last sent event from the queue
                                    eventQueue.pop();
                            }

                            //if I have something to read
                            if (FD_ISSET(client_socket.getSockfd(), &read_fds)) {
                                //receive server message and process it
                                pm.receive();
                            }
                    }
                }

                //explicitly close connection
                client_socket.closeConnection();
            }
            catch (SocketException &e) {
                switch (e.getCode()) {
                    case SocketError::connect:
                    case SocketError::read:
                    case SocketError::write:
                    case SocketError::closed:

                        //if this is the first retry (and it is not the first connection I make)
                        if(tries == 0 && connectionCounter != 0)
                            Message::print(std::cout, "INFO", "Connection was closed by the server",
                                           "will reconnect if needed");

                        //if there are not messages waiting for a serer response
                        //and there are not new events to send
                        if(!pm.isWaiting() && !eventQueue.canGet())
                            //just break out
                            break;

                        //otherwise

                        //if the persist option was set I want to retry the connection indefinitely
                        if(persist) {
                            //retry later, after x seconds

                            tries++;    //update retries

                            std::stringstream tmp;
                            tmp << "Retry (" << tries << ") in " << config->getSecondsBetweenReconnections()
                                << " seconds.";

                            Message::print(std::cerr, "WARNING", "Connection error", tmp.str());

                            //sleep for some time before retrying the connection
                            std::this_thread::sleep_for(std::chrono::seconds(config->getSecondsBetweenReconnections()));
                            break;
                        }

                        //otherwise

                        //re-try only for a limited number of times

                        //if tries is less than MaxConnectionRetries
                        if (tries < config->getMaxConnectionRetries()) {
                            //retry later, after x seconds

                            tries++;    //update retries

                            std::stringstream tmp;
                            tmp << "Retry (" << tries << ") in " << config->getSecondsBetweenReconnections()
                                << " seconds.";

                            Message::print(std::cerr, "WARNING", "Connection error", tmp.str());

                            //sleep for some time before retrying the connection
                            std::this_thread::sleep_for(std::chrono::seconds(config->getSecondsBetweenReconnections()));
                            break;
                        }

                        //otherwise

                        //maximum number of re-tries exceeded -> terminate program

                        Message::print(std::cerr, "ERROR", "Cannot establish connection");

                    case SocketError::create:
                    case SocketError::accept:
                    case SocketError::bind:
                    case SocketError::getMac:
                    case SocketError::getIp:
                    default:
                        //print a message and exit

                        Message::print(std::cerr, "ERROR", "Socket Exception", e.what());

                        //terminate filesystem watcher and exit
                        fileWatcher_stop.store(true);

                        //notify file watcher thread
                        eventQueue.notifyAll();

                        return;
                }
            }
        }
    }
    catch (ConfigException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case ConfigError::justCreated:
            case ConfigError::pathToWatch:
                Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

            case ConfigError::path:
            case ConfigError::open:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Config Exception", e.what());

                //terminate filesystem watcher and exit
                fileWatcher_stop.store(true);

                //notify file watcher thread
                eventQueue.notifyAll();

                return;
        }
    }
    catch (DatabaseException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case DatabaseError::path:
            case DatabaseError::create:
            case DatabaseError::open:
            case DatabaseError::prepare:
            case DatabaseError::finalize:
            case DatabaseError::insert:
            case DatabaseError::read:
            case DatabaseError::update:
            case DatabaseError::remove:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Database Exception", e.what());

                //terminate filesystem watcher and exit
                fileWatcher_stop.store(true);

                //notify file watcher thread
                eventQueue.notifyAll();

                return;
        }
    }
    catch (ProtocolManagerException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case ProtocolManagerError::auth:
            case ProtocolManagerError::internal:
            case ProtocolManagerError::client:
            case ProtocolManagerError::serverMessage:
            case ProtocolManagerError::version:
            case ProtocolManagerError::unexpected:
            case ProtocolManagerError::unexpectedCode:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "ProtocolManager Exception", e.what());

                //terminate filesystem watcher and exit
                fileWatcher_stop.store(true);

                //notify file watcher thread
                eventQueue.notifyAll();

                return;
        }
    }
    catch (SocketException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case SocketError::create:
            case SocketError::accept:
            case SocketError::bind:
            case SocketError::read:
            case SocketError::write:
            case SocketError::connect:
            case SocketError::getMac:
            case SocketError::getIp:
            case SocketError::closed:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Socket Exception", e.what());

                //terminate filesystem watcher and exit
                fileWatcher_stop.store(true);

                //notify file watcher thread
                eventQueue.notifyAll();

                return;
        }
    }
    catch (HashException &e) {
        //switch on the exception code
        switch(e.getCode()){
            case HashError::init:
            case HashError::update:
            case HashError::finalize:
            case HashError::set:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Hash Exception", e.what());

                //terminate filesystem watcher and exit
                fileWatcher_stop.store(true);

                //notify file watcher thread
                eventQueue.notifyAll();

                return;
        }
    }
    catch (RngException &e) {
        //switch on the exception code
        switch(e.getCode()) {
            case RngError::init:
            case RngError::generate:
            default:
                //print a message and exit

                Message::print(std::cerr, "ERROR", "Rng Exception", e.what());

                //terminate filesystem watcher and exit
                fileWatcher_stop.store(true);

                //notify file watcher thread
                eventQueue.notifyAll();

                return;
        }
    }
    catch (std::exception &e) {
        //print a message and exit

        Message::print(std::cerr, "ERROR", "generic exception", e.what());

        //terminate filesystem watcher and exit
        fileWatcher_stop.store(true);

        //notify file watcher thread
        eventQueue.notifyAll();

        return;
    }
    catch (...) {
        //print a message and exit

        Message::print(std::cerr, "ERROR", "uncaught exception");

        //terminate filesystem watcher and close program
        fileWatcher_stop.store(true);

        //notify file watcher thread
        eventQueue.notifyAll();

        return;
    }
}