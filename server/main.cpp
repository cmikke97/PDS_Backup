//
// Created by Michele Crepaldi s269551 on 25/07/2020
// Finished on 22/11/2020
// Last checked on 22/11/2020
//


#include <messages.pb.h>

#include <iostream>
#include <thread>
#include <regex>

#include "../myLibraries/Socket.h"
#include "../myLibraries/Circular_vector.h"
#include "../myLibraries/Message.h"
#include "../myLibraries/Hash.h"
#include "../myLibraries/RandomNumberGenerator.h"
#include "../myLibraries/Validator.h"

#include "Thread_guard.h"
#include "Database_pwd.h"
#include "Database.h"
#include "Config.h"
#include "ProtocolManager.h"
#include "ArgumentsManager.h"


#define VERSION 1

#define PORT 8081
#define SOCKET_TYPE SocketType::TLS
#define CONFIG_FILE_PATH "../config.txt"

using namespace server;

//function representing a single server thread
void single_server(TS_Circular_vector<std::pair<std::string, Socket>> &, std::atomic<bool> &, std::atomic<bool> &);

/**
 * main function
 *
 * @param argc number of main arguments
 * @param argv main arguments
 * @return exit value
 *
 * @author Michele Crepaldi s269551
 */
int main(int argc, char** argv) {
    //verify that the version of the library that we linked against is
    //compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    try{
        //get options and option arguments from the main arguments

        ArgumentsManager inputArgs{argc, argv}; //main input arguments

        Config::setPath(CONFIG_FILE_PATH);  //set the configuration file path
        auto config = Config::getInstance();    //config instance

        Database::setPath(config->getServerDatabasePath());         //set the database path
        Database_pwd::setPath(config->getPasswordDatabasePath());   //set the password database path
        auto db = Database::getInstance();          //server database instance
        auto pass_db = Database_pwd::getInstance(); //password database instance

        if(inputArgs.isAddSet()){   //if addUser option is set
            //add user (and password) to the password database

            pass_db->addUser(inputArgs.getUsername(), inputArgs.getPassword());
            Message::print(std::cout, "SUCCESS", "User " + inputArgs.getUsername()
                            + " added to server.");
        }

        if(inputArgs.isUpdateSet()){    //if updateUser option is set
            //update user password in the password database

            pass_db->updateUser(inputArgs.getUsername(), inputArgs.getPassword());
            Message::print(std::cout, "SUCCESS", "User " + inputArgs.getUsername()
                            + " updated on server.");
        }

        if(inputArgs.isRemoveSet()){    //if removeUser option is set
            //remove user from the password database and
            //delete all the related elements both in database and filesystem

            pass_db->removeUser(inputArgs.getUsername());
            Message::print(std::cout, "SUCCESS", "User " + inputArgs.getUsername()
                            + " removed from server.");

            //now remove all the elements saved for the removed user from database and from filesystem

            //all the mac addresses associated to the user
            std::vector<std::string> macAddrs = db->getAllMacAddresses(inputArgs.getUsername());

            //remove all backup elements related to the user from server database
            db->removeAll(inputArgs.getUsername());

            for(auto mac: macAddrs){    //for each mac address of that user
                //compute the backup folder name

                std::stringstream backupFolderName;
                backupFolderName << config->getServerBasePath() << "/" << inputArgs.getUsername()
                    << "_" << std::regex_replace(mac, std::regex(":"), "-");

                //remove all the elements in the user's backup folder corresponding to the current mac
                std::filesystem::remove_all(backupFolderName.str());
            }

            Message::print(std::cout, "SUCCESS", "All " + inputArgs.getUsername()
                            + " backups deleted.");
        }

        if(inputArgs.isViewSet()){  //if viewUsers option is set
            //view all registered users

            Message::print(std::cout, "INFO", "Registered Users:");

            //function to be used for each element of the password database
            std::function<void (const std::string &)> f = [](const std::string &username){
                std::cout << "\t" << username << std::endl;
            };

            //apply function on all elements of the password database
            pass_db->forAll(f);
        }

        if(inputArgs.isDeleteSet()){    //if delete option is set
            //delete all or some backed up elements for the specified user both from database and from filesystem

            if(inputArgs.isMacSet()){   //if a mac address is set
                //delete all backup elements for the specified user and mac from server database

                db->removeAll(inputArgs.getDelUsername(), inputArgs.getDelMac());

                //compute the backup folder name

                std::stringstream backupFolderName;
                backupFolderName << config->getServerBasePath() << "/" << inputArgs.getDelUsername()
                    << "_" << std::regex_replace(inputArgs.getDelMac(), std::regex(":"), "-");

                //remove all the elements in the user's backup folder corresponding to the current mac
                std::filesystem::remove_all(backupFolderName.str());

                Message::print(std::cout, "SUCCESS", "All elements in " + inputArgs.getDelUsername()
                                + "@" + inputArgs.getDelMac() + " backup deleted.");
            }
            else{   //otherwise
                //delete all backup elements for the specified user (ALL OF THEM!)

                //all the mac addresses associated to the user
                std::vector<std::string> macAddrs = db->getAllMacAddresses(inputArgs.getDelUsername());

                //remove all backup elements related to the user from server database
                db->removeAll(inputArgs.getDelUsername());

                //for each mac address
                for(auto mac: macAddrs){
                    //compute the backup folder name

                    std::stringstream backupFolderName;
                    backupFolderName << config->getServerBasePath() << "/" << inputArgs.getDelUsername() << "_"
                        << std::regex_replace(mac, std::regex(":"), "-");

                    //remove all the elements in the user's backup folder corresponding to the current mac
                    std::filesystem::remove_all(backupFolderName.str());
                }

                Message::print(std::cout, "SUCCESS", "All " + inputArgs.getDelUsername()
                                + " backups deleted.");
            }
        }

        if(inputArgs.isStartSet()) { //if the start option is set
            //start the service

            Message::print(std::cout, "SERVICE", "Starting service..");
            Message::print(std::cout, "INFO", "Server base path:", config->getServerBasePath());

            //specify the server certificate, server private key and CA certificate paths
            //to be used by the TLS sockets
            ServerSocket::specifyCertificates(config->getCertificatePath(), config->getPrivateKeyPath(),
                                              config->getCaFilePath());

            ServerSocket server_sock{PORT, config->getListenQueue(), SOCKET_TYPE};   //server socket

            Message::print(std::cout, "INFO", "Server opened: available at", "["
                            + std::string(server_sock.getIP()) + ":" + std::to_string(PORT) + "]");

            //thread safe circular vector of fixed size containing a list of sockets and their ip addresses
            TS_Circular_vector<std::pair<std::string, Socket>> sockets{config->getSocketQueueSize()};

            std::atomic<bool> server_threads_stop = false;  //atomic boolean used to force the server threads to stop
            std::atomic<bool> main_stop = false;            //atomic boolean used to force the main thread to stop
            std::vector<std::thread> threads;   //vector containing all server threads

            //create NThreads threads and start them
            for (int i = 0; i < config->getNThreads(); i++)
                threads.emplace_back(single_server, std::ref(sockets), std::ref(server_threads_stop),
                                     std::ref(main_stop));

            //thread guard used to stop all the threads in the threads vector when the main returns
            Thread_guard td{threads, sockets, server_threads_stop};

            struct sockaddr_in addr{};  //client sockaddr_in structure
            unsigned long addr_len = sizeof(addr);  //length of the sockaddr_in structure

            //loop until told to stop (by the server threads, in case of a Fatal exception)
            while (!main_stop.load()) {
                //accept a new client socket

                Socket s = server_sock.accept(&addr, &addr_len);    //socket of incoming client connection

                //if told to stop return -> the stopping server thread (the one that will set this variable)
                //will also connect locally to this socket in order to exit from the accept
                if(main_stop.load())
                    return 0;

                //obtain ip address of newly connected client

                std::stringstream tmp;
                tmp << inet_ntoa(addr.sin_addr) << ":" << addr.sin_port;
                std::string clientAddress = tmp.str();

                //push the (address,socket) pair into the socket queue
                sockets.push(std::make_pair(std::move(clientAddress),std::move(s)));
            }
        }
    }
    catch (ArgumentsManagerException &e) {  //catch arguments manager exceptions
        //switch on the exception code
        switch(e.getCode()){
            case ArgumentsManagerError::help:
                return 0;
            case ArgumentsManagerError::numberOfArguments:
            case ArgumentsManagerError::option:
            case ArgumentsManagerError::optArgument:
            default:
                //print error and exit

                Message::print(std::cerr, "ERROR", "ArgumentsManager Exception", e.what());
                return 1;
        }
    }
    catch (ConfigException &e) {    //catch config exceptions
        //switch on the exception code
        switch(e.getCode()){
            case ConfigError::justCreated:
            case ConfigError::serverBasePath:
            case ConfigError::tempPath:
                //prompt the user to check the configuration file
                Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

            case ConfigError::path:
            case ConfigError::open:
            default:
                //print message and exit

                Message::print(std::cerr, "ERROR", "Config Exception", e.what());
                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (DatabaseException &e) {  //catch database exceptions
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
                //print message and exit

                Message::print(std::cerr, "ERROR", "Database Exception", e.what());

                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (DatabaseException_pwd &e) {  //catch password database exceptions
        //switch on the exception code
        switch(e.getCode()){
            case DatabaseError_pwd::insert:
                Message::print(std::cerr, "ERROR", "PWD_Database Exception", "User already exists.");

            case DatabaseError_pwd::path:
            case DatabaseError_pwd::create:
            case DatabaseError_pwd::open:
            case DatabaseError_pwd::prepare:
            case DatabaseError_pwd::finalize:
            case DatabaseError_pwd::read:
            case DatabaseError_pwd::update:
            case DatabaseError_pwd::remove:
            default:
                //print message and exit

                Message::print(std::cerr, "ERROR", "PWD_Database Exception", e.what());

                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (SocketException &e) {    //catch socket exceptions
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
                //print message and exit

                Message::print(std::cerr, "ERROR", "Socket Exception", e.what());
                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (HashException &e) {  //catch hash exceptions
        //switch on the exception code
        switch(e.getCode()){
            case HashError::init:
            case HashError::update:
            case HashError::finalize:
            case HashError::set:
            default:
                //print message and exit

                Message::print(std::cerr, "ERROR", "Hash exception", e.what());
                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (RngException &e) {   //catch random number generator exceptions
        //switch on the exception code
        switch(e.getCode()){
            case RngError::init:
            case RngError::generate:
            default:
                //print message and exit

                Message::print(std::cerr, "ERROR", "Rng exception", e.what());
                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (std::exception &e) {
        //print message and exit

        Message::print(std::cerr, "ERROR", "exception", e.what());
        return 1;
    }
    catch (...) {
        //print message and exit

        Message::print(std::cerr, "ERROR", "uncaught exception");
        return 1;
    }

    return 1;
}

/**
 * single server function.
 *  function representing a single server thread. Used to manage the connection with a client
 *
 * @param sockets list of (address,socket) pairs
 * @param main_stop atomic boolean to stop the main thread
 * @param server_stop atomic boolean to stop the server threads
 *
 * @author Michele Crepaldi s269551
 */
void single_server(TS_Circular_vector<std::pair<std::string, Socket>> &sockets, std::atomic<bool> &server_threads_stop,
                   std::atomic<bool> &main_stop){

    //loop until we are told to stop
    while(!server_threads_stop.load()){

        //get the first socket in the socket queue (removing it from the queue);
        //if no element is present then passively wait until an element can be removed or the server_threads_stop
        //boolean becomes true (in such case returning a nullopt)

        auto optional = sockets.tryGetUntil(server_threads_stop);   //optional (address,socket) pair

        //if the optional does not have a value it means that the previous function returned
        //because of server_threads_stop being true, so I need to return
        if(!optional.has_value())
            return;

        auto pair = std::move(optional.value());    //(address,socket) pair from optional

        std::string address = pair.first;       //address of the client connected to the socket
        Socket sock = std::move(pair.second);   //client socket

        //print ip address and port of the newly connected client
        Message::print(std::cout, "EVENT", address, "New Connection");

        try{
            auto config = Config::getInstance();    //config instance

            ProtocolManager pm{sock, address, VERSION}; //protocol manager for this connection

            //authenticate the connected client
            pm.authenticate();

            //if authentication is successful

            fd_set read_fds;        //fd read set for select
            int timeWaited = 0;     //total time waited without receiving messages from client
            bool loop = true;       //loop condition

            //loop until we are told to stop or loop condition becomes false
            while(loop && !server_threads_stop.load()) {
                //build fd sets
                FD_ZERO(&read_fds);
                FD_SET(sock.getSockfd(), &read_fds);

                int maxfd = sock.getSockfd();   //select max fd
                struct timeval tv{};    //timeval struct for select
                tv.tv_sec = config->getSelectTimeoutSeconds();  //set timeval for the select function

                //select on read socket

                int activity = select(maxfd + 1, &read_fds, nullptr, nullptr, &tv);

                switch (activity) {
                    case -1:
                        //I should never get here
                        Message::print(std::cerr, "ERROR", "Select error");

                        //exit loop --> this will cause the current connection to be closed
                        loop = false;
                        break;

                    case 0:
                        //update time waited
                        timeWaited += config->getSelectTimeoutSeconds();

                        //if the time already waited is greater than TimeoutSeconds
                        if (timeWaited >= config->getTimeoutSeconds()) {
                            //then exit loop --> this will cause the current connection to be closed

                            loop = false;
                            Message::print(std::cout, "INFO", "Disconnecting client ", address);
                        }

                        break;

                    default:
                        //reset timeout
                        timeWaited = 0;

                        //if I have something to read
                        if (FD_ISSET(sock.getSockfd(), &read_fds)) {
                            //receive client message and process it
                            pm.receive();
                        }
                }
            }
        }
        catch (ConfigException &e) {
            //switch on the exception code
            switch(e.getCode()){
                case ConfigError::justCreated:
                case ConfigError::serverBasePath:
                case ConfigError::tempPath:
                    //prompt the user to check the configuration file
                    Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

                case ConfigError::path:
                case ConfigError::open:
                default:
                    //print message and exit

                    Message::print(std::cerr, "ERROR", "Config Exception", e.what());

                    //set the stop atomic boolean for the main (the main will stop all the other threads)
                    main_stop.store(true);

                    Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

                    //connect to the local serverSocket in order to make it exit the accept
                    tmp.connect("localhost",PORT);

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
                    //print message and exit

                    Message::print(std::cerr, "ERROR", "Database Exception", e.what());

                    //set the stop atomic boolean for the main (the main will stop all the other threads)
                    main_stop.store(true);

                    Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

                    //connect to the local serverSocket in order to make it exit the accept
                    tmp.connect("localhost",PORT);

                    return;
            }
        }
        catch (DatabaseException_pwd &e) {
            //switch on the exception code
            switch(e.getCode()){
                //these errors should not be triggered in the server threads
                case DatabaseError_pwd::path:
                case DatabaseError_pwd::create:
                case DatabaseError_pwd::open:
                case DatabaseError_pwd::prepare:
                case DatabaseError_pwd::finalize:
                case DatabaseError_pwd::insert:
                case DatabaseError_pwd::update:
                case DatabaseError_pwd::remove:

                    //this is the only password database error that could happen in the server threads
                case DatabaseError_pwd::read:
                default:
                    //print message and exit

                    Message::print(std::cerr, "ERROR", "PWD_Database Exception", e.what());

                    //set the stop atomic boolean for the main (the main will stop all the other threads)
                    main_stop.store(true);

                    Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

                    //connect to the local serverSocket in order to make it exit the accept
                    tmp.connect("localhost",PORT);

                    return;
            }
        }
        catch (ProtocolManagerException &e) {
            //switch on the exception code
            switch(e.getCode()){
                //these 2 cases are handled directly by the protocol manager -> keep connection and skip message;
                case ProtocolManagerError::unexpected:
                case ProtocolManagerError::client:

                //in these 2 cases connection with the client is not valid and needs to be closed
                case ProtocolManagerError::auth:
                case ProtocolManagerError::version:
                    //print message and close connection, then proceed with next client connection (loop)

                    Message::print(std::cerr, "WARNING", "ProtocolManager Exception", e.what());

                    Message::print(std::cout, "INFO", "Closing connection with client",
                                   "I will proceed with next connections");

                    //the connection will be closed automatically by the destructor
                    //(since sock is declared inside the loop)

                    continue;   //the error is in the current socket, continue with the next one

                //in this case (and default) the errors are so important that they require
                //the closing of the whole program
                case ProtocolManagerError::internal:
                default:
                    //print message and exit

                    Message::print(std::cerr, "ERROR", "ProtocolManager Exception", e.what());

                    //set the stop atomic boolean for the main (the main will stop all the other threads)
                    main_stop.store(true);

                    Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

                    //connect to the local serverSocket in order to make it exit the accept
                    tmp.connect("localhost",PORT);

                    return;
            }
        }
        catch (SocketException &e) {
            //switch on the exception code
            switch(e.getCode()){
                case SocketError::read:
                case SocketError::write:
                case SocketError::closed:
                    Message::print(std::cout, "EVENT", address, "disconnected.");
                    continue;   //error in the current socket, continue with the next one

                //these errors should not be triggered in the server threads
                case SocketError::create:
                case SocketError::accept:
                case SocketError::bind:
                case SocketError::connect:
                case SocketError::getMac:
                case SocketError::getIp:
                default:
                    //print message and exit

                    Message::print(std::cerr, "ERROR", "Socket Exception", e.what());

                    //set the stop atomic boolean for the main (the main will stop all the other threads)
                    main_stop.store(true);

                    Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

                    //connect to the local serverSocket in order to make it exit the accept
                    tmp.connect("localhost",PORT);

                    return;
            }
        }
        catch (HashException &e) {  //catch hash exceptions
            //switch on the exception code
            switch(e.getCode()){
                case HashError::init:
                case HashError::update:
                case HashError::finalize:
                case HashError::set:
                default:
                    //print message and exit

                    Message::print(std::cerr, "ERROR", "Hash exception", e.what());

                    //set the stop atomic boolean for the main (the main will stop all the other threads)
                    main_stop.store(true);

                    Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

                    //connect to the local serverSocket in order to make it exit the accept
                    tmp.connect("localhost",PORT);

                    return;
            }
        }
        catch (RngException &e) {   //catch random number generator exceptions
            //switch on the exception code
            switch(e.getCode()){
                case RngError::init:
                case RngError::generate:
                default:
                    //print message and exit

                    Message::print(std::cerr, "ERROR", "Rng exception", e.what());

                    //set the stop atomic boolean for the main (the main will stop all the other threads)
                    main_stop.store(true);

                    Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

                    //connect to the local serverSocket in order to make it exit the accept
                    tmp.connect("localhost",PORT);

                    return;
            }
        }
        catch (std::exception &e) {
            //print message and exit

            Message::print(std::cerr, "ERROR", "generic exception", e.what());

            //set the stop atomic boolean for the main (the main will stop all the other threads)
            main_stop.store(true);

            Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

            //connect to the local serverSocket in order to make it exit the accept
            tmp.connect("localhost",PORT);

            return;
        }
        catch (...) {
            //print message and exit

            Message::print(std::cerr, "ERROR", "uncaught exception");

            //set the stop atomic boolean for the main (the main will stop all the other threads)
            main_stop.store(true);

            Socket tmp{SocketType::TCP};    //temporary socket to close wake the main

            //connect to the local serverSocket in order to make it exit the accept
            tmp.connect("localhost",PORT);

            return;
        }
    }
}