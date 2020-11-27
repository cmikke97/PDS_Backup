//
// Created by michele on 25/07/2020.
//

#include <iostream>
#include <messages.pb.h>
#include <sstream>
#include <thread>
#include <regex>
#include "../myLibraries/Socket.h"
#include "../myLibraries/Circular_vector.h"
#include "Thread_guard.h"
#include "Database_pwd.h"
#include "Database.h"
#include "Config.h"
#include "ProtocolManager.h"
#include "../myLibraries/Message.h"
#include "../myLibraries/Validator.h"
#include "ArgumentsManager.h"

//TODO check

#define VERSION 1

#define PORT 8081
#define SOCKET_TYPE SocketType::TLS
#define CONFIG_FILE_PATH "../config.txt"

void single_server(TS_Circular_vector<std::pair<std::string, Socket>> &, std::atomic<bool> &, std::atomic<bool> &);

int main(int argc, char** argv) {
    //verify that the version of the library that we linked against is
    //compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    try{
        server::ArgumentsManager inputArgs = server::ArgumentsManager(argc, argv);  //main input arguments

        server::Config::setPath(CONFIG_FILE_PATH);
        auto config = server::Config::getInstance();
        server::Database::setPath(config->getServerDatabasePath()); //set the database path
        server::Database_pwd::setPath(config->getPasswordDatabasePath());   //set the password database path
        auto pass_db = server::Database_pwd::getInstance();
        auto db = server::Database::getInstance();

        if(inputArgs.isAddSet()){   //add user (and password) to the server
            pass_db->addUser(inputArgs.getUsername(), inputArgs.getPassword());
            Message::print(std::cout, "SUCCESS", "User " + inputArgs.getUsername() + " added to server.");
        }

        if(inputArgs.isUpdateSet()){    //update user password in the server
            pass_db->updateUser(inputArgs.getUsername(), inputArgs.getPassword());
            Message::print(std::cout, "SUCCESS", "User " + inputArgs.getUsername() + " updated on server.");
        }

        if(inputArgs.isRemoveSet()){    //remove user from the server
            pass_db->removeUser(inputArgs.getUsername());
            Message::print(std::cout, "SUCCESS", "User " + inputArgs.getUsername() + " removed from server.");

            std::vector<std::string> macAddrs = db->getAllMacAddresses(inputArgs.getUsername());    //get all the mac addresses associated to the user

            db->removeAll(inputArgs.getUsername());    //remove all backup elements related to that user

            for(auto mac: macAddrs){
                //compute the backup folder name
                std::stringstream tmp;
                tmp << config->getServerBasePath() << "/" << inputArgs.getUsername() << "_" << std::regex_replace(mac, std::regex(":"), "-");
                std::filesystem::remove_all(tmp.str()); //remove all the elements in the user's backup folder corresponding to that mac
            }

            Message::print(std::cout, "SUCCESS", "All " + inputArgs.getUsername() + " backups deleted.");
        }

        if(inputArgs.isViewSet()){  //view all users registered in the server
            Message::print(std::cout, "INFO", "Registered Users:");
            std::function<void (const std::string &)> f = [](const std::string &username){
                std::cout << "\t" << username << std::endl;
            };
            pass_db->forAll(f);
        }

        if(inputArgs.isDeleteSet()){
            if(inputArgs.isMacSet()){ //if a mac address is set delete all backup elements for the specified user and mac
                db->removeAll(inputArgs.getDelUsername(), inputArgs.getDelMac());

                //compute the backup folder name
                std::stringstream tmp;
                tmp << config->getServerBasePath() << "/" << inputArgs.getDelUsername() << "_" << std::regex_replace(inputArgs.getDelMac(), std::regex(":"), "-");
                std::filesystem::remove_all(tmp.str()); //remove all the elements in the user's backup folder corresponding to that mac

                Message::print(std::cout, "SUCCESS", "All elements in " + inputArgs.getDelUsername() + "@" + inputArgs.getDelMac() + " backup deleted.");
            }
            else{   //else delete all backups elements for the specified user (ALL OF THEM!)
                std::vector<std::string> macAddrs = db->getAllMacAddresses(inputArgs.getDelUsername());    //get all the mac addresses associated to the user

                db->removeAll(inputArgs.getDelUsername()); //delete all elements for the specified user

                for(auto mac: macAddrs){
                    //compute the backup folder name
                    std::stringstream tmp;
                    tmp << config->getServerBasePath() << "/" << inputArgs.getDelUsername() << "_" << std::regex_replace(mac, std::regex(":"), "-");
                    std::filesystem::remove_all(tmp.str()); //remove all the elements in the user's backup folder corresponding to that mac
                }

                Message::print(std::cout, "SUCCESS", "All " + inputArgs.getDelUsername() + " backups deleted.");
            }
        }

        if(inputArgs.isStartSet()) { //start the server
            Message::print(std::cout, "SERVICE", "Starting service..");
            Message::print(std::cout, "INFO", "Server base path:", config->getServerBasePath());
            ServerSocket::specifyCertificates(config->getCertificatePath(), config->getPrivateKeyPath(), config->getCaFilePath());
            ServerSocket server{PORT, config->getListenQueue(), SOCKET_TYPE};   //initialize server socket with port

            Message::print(std::cout, "INFO", "Server opened: available at", "[" + std::string(server.getIP()) + ":" + std::to_string(PORT) + "]");

            TS_Circular_vector<std::pair<std::string, Socket>> sockets{config->getSocketQueueSize()};

            std::atomic<bool> server_thread_stop = false, main_stop = false;   //atomic boolean to force the server threads to stop
            std::vector<std::thread> threads;

            for (int i = 0; i < config->getNThreads(); i++)  //create N_THREADS threads and start them
                threads.emplace_back(single_server, std::ref(sockets), std::ref(server_thread_stop), std::ref(main_stop));

            server::Thread_guard td{threads, sockets, server_thread_stop};

            struct sockaddr_in addr{};
            unsigned long addr_len = sizeof(addr);

            while (!main_stop.load()) { //loop until told to stop
                //alternative: wait here for free space in circular vector and then accept and push

                Socket s = server.accept(&addr, &addr_len); //accept a new client socket
                //if told to stop return -> the stopping server thread (the one that will set this variable)
                //will also connect locally to this socket in order to exit from the accept
                if(main_stop.load())
                    return 0;

                std::stringstream tmp;
                tmp << inet_ntoa(addr.sin_addr) << ":" << addr.sin_port; //obtain ip address of newly connected client
                std::string clientAddress = tmp.str();
                sockets.push(std::make_pair(std::move(clientAddress),std::move(s))); //push the socket into the socket queue
            }
        }
    }
    catch (server::ArgumentsManagerException &e) {
        switch(e.getCode()){
            case server::ArgumentsManagerError::help:
                return 0;
            case server::ArgumentsManagerError::numberOfArguments:
            case server::ArgumentsManagerError::option:
            case server::ArgumentsManagerError::optArgument:
            default:
                Message::print(std::cerr, "ERROR", "Input Exception", e.what());

                return 1;
        }
    }
    catch (SocketException &e) {
        switch(e.getCode()){
            case SocketError::create:
            case SocketError::accept:
            default:
                Message::print(std::cerr, "ERROR", "Socket Exception", e.what());
                return 1;
        }
    }
    catch (server::DatabaseException_pwd &e) {
        switch(e.getCode()){
            case server::DatabaseError_pwd::insert: //could not insert into the database -> (fatal) terminate server (not used here)
                Message::print(std::cerr, "ERROR", "PWD_Database Exception", "User already exists.");
            case server::DatabaseError_pwd::path:   //no path was provided to the Database_pwd class
            case server::DatabaseError_pwd::create: //could not create the table in the database -> (fatal) terminate server
            case server::DatabaseError_pwd::open:   //could not open the database -> (fatal) terminate server
            case server::DatabaseError_pwd::prepare:    //could not prepare a SQL statement -> (fatal) terminate server
            case server::DatabaseError_pwd::finalize:   //could not finalize SQL statement -> (fatal) terminate server
            case server::DatabaseError_pwd::read:   //could not read from the database -> (fatal) terminate server (not used here)
            case server::DatabaseError_pwd::update: //could not update into the database -> (fatal) terminate server (not used here)
            case server::DatabaseError_pwd::remove: //could not remove from the database -> (fatal) terminate server (not used here)
            default:
                //Fatal error -> close the server
                Message::print(std::cerr, "ERROR", "PWD_Database Exception", e.what());

                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (server::DatabaseException &e) {
        switch(e.getCode()){
            case server::DatabaseError::path:   //no path was provided to the Database class
            case server::DatabaseError::create: //could not create the table in the database -> (fatal) terminate server
            case server::DatabaseError::open:   //could not open the database -> (fatal) terminate server
            case server::DatabaseError::prepare:    //could not prepare a SQL statement -> (fatal) terminate server
            case server::DatabaseError::finalize:   //could not finalize SQL statement -> (fatal) terminate server
            case server::DatabaseError::insert: //could not insert into the database -> (fatal) terminate server (not used here)
            case server::DatabaseError::read:   //could not read from the database -> (fatal) terminate server (not used here)
            case server::DatabaseError::update: //could not update into the database -> (fatal) terminate server (not used here)
            case server::DatabaseError::remove: //could not remove from the database -> (fatal) terminate server (not used here)
            default:
                //Fatal error -> close the server
                Message::print(std::cerr, "ERROR", "Database Exception", e.what());

                return 1; //return -> the thread guard will stop and join all the server threads
        }
    }
    catch (server::ConfigException &e) {
        switch(e.getCode()){
            case server::ConfigError::justCreated:  //if the config file was not there and it has been created
            case server::ConfigError::serverBasePath:  //if the configured server base path was not specified or it does not exist (or it is not a directory) ask to modify it and return
            case server::ConfigError::tempPath: //if the configured server temporary path was not specified ask to modify it and return
                Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

            case server::ConfigError::open: //if there were some errors in opening the configuration file return
            default:
                Message::print(std::cerr, "ERROR", "Config Exception", e.what());
                return 1;
        }
    }
    catch (std::exception &e) {
        Message::print(std::cerr, "ERROR", "exception", e.what());
        return 1;
    }

    return 0;
}

void single_server(TS_Circular_vector<std::pair<std::string, Socket>> &sockets, std::atomic<bool> &thread_stop, std::atomic<bool> &server_stop){
    while(!thread_stop.load()){ //loop until we are told to stop
        //Socket sock = sockets.get();    //get the first socket in the socket queue (removing it from the queue); if no element is present then passively wait

        //TODO check
        auto optional = sockets.tryGetUntil(thread_stop);   //get the first socket in the socket queue (removing it from the queue); if no element is present then passively wait

        if(!optional.has_value())   //TODO check
            return;

        auto pair = std::move(optional.value());

        std::string address = pair.first;
        Socket sock = std::move(pair.second);

        Message::print(std::cout, "EVENT", address, "New Connection");   //print ip address and port of the newly connected client

        //for select
        fd_set read_fds;
        int timeWaited = 0;
        bool loop = true;

        try{
            server::Config::setPath(CONFIG_FILE_PATH);
            auto config = server::Config::getInstance();

            server::ProtocolManager pm{sock, address, VERSION};

            //authenticate the connected client
            pm.authenticate();

            while(loop && !thread_stop.load()) { //loop until we are told to stop
                //build fd sets
                FD_ZERO(&read_fds);
                FD_SET(sock.getSockfd(), &read_fds);

                int maxfd = sock.getSockfd();
                struct timeval tv{};
                tv.tv_sec = config->getSelectTimeoutSeconds();

                //select on read socket
                int activity = select(maxfd + 1, &read_fds, nullptr, nullptr, &tv);

                switch (activity) {
                    case -1:
                        //I should never get here
                        Message::print(std::cerr, "ERROR", "Select error");

                        loop = false;      //exit loop --> this will cause the current connection to be closed
                        break;

                    case 0:
                        timeWaited += config->getSelectTimeoutSeconds();

                        if (timeWaited >= config->getTimeoutSeconds()) {  //if the time already waited is greater than TIMEOUT
                            loop = false;      //then exit loop --> this will cause the current connection to be closed
                            Message::print(std::cout, "INFO", "Disconnecting client ", address);
                        }

                        break;

                    default:
                        //reset timeout
                        timeWaited = 0;

                        //if I have something to read
                        if (FD_ISSET(sock.getSockfd(), &read_fds)) {
                            pm.receive();
                        }
                }
            }
        }
        catch (server::ProtocolManagerException &e) {
            switch(e.getCode()){
                //these 2 cases are handled directly by the protocol manager -> keep connection and skip message;
                //anyway if they appear here close connection and continue with the next socket
                case server::ProtocolManagerError::unexpected:
                case server::ProtocolManagerError::client:

                //in these 2 cases connection with the client is not valid and needs to be closed
                case server::ProtocolManagerError::auth:
                case server::ProtocolManagerError::version:
                    Message::print(std::cerr, "WARNING", "ProtocolManager Exception", e.what());

                    sock.closeConnection(); //close the connection with the client

                    Message::print(std::cout, "INFO", "Closing connection with client", "I will proceed with next connections");
                    continue;   //the error is in the current socket, continue with the next one

                //in this case (and default) the errors are so important that they require the closing of the whole program
                case server::ProtocolManagerError::internal:
                default:
                    //Fatal error -> close the server
                    Message::print(std::cerr, "ERROR", "ProtocolManager Exception", e.what());

                    server_stop.store(true);    //set the stop atomic boolean for the main (the main will stop all the other threads)

                    Socket tmp{SocketType::TCP};
                    tmp.connect("localhost",PORT); //connect to the local serverSocket in order to make it exit the accept

                    return; //then return
            }
        }
        catch (SocketException &e) {
            switch(e.getCode()){
                case SocketError::read:
                case SocketError::write:
                case SocketError::closed:
                    Message::print(std::cout, "EVENT", address, "disconnected.");
                    continue; //error in the current socket, continue with the next one

                //added for completeness, will never be triggered in the server threads
                case SocketError::create:
                case SocketError::accept:
                case SocketError::bind:
                case SocketError::connect:
                case SocketError::getMac:
                case SocketError::getIP:
                default:
                    //Fatal error -> close the server
                    Message::print(std::cerr, "ERROR", "Socket Exception", e.what());

                    server_stop.store(true);    //set the stop atomic boolean for the main (the main will stop all the other threads)

                    Socket tmp{SocketType::TCP};
                    tmp.connect("localhost",PORT); //connect to the local serverSocket in order to make it exit the accept

                    return; //then return
            }
        }
        catch (server::DatabaseException_pwd &e) {
            switch(e.getCode()){
                //these errors were added for completeness but will never be triggered in the server threads
                case server::DatabaseError_pwd::create: //could not create the table in the database -> (fatal) terminate server
                case server::DatabaseError_pwd::open:   //could not open the database -> (fatal) terminate server
                case server::DatabaseError_pwd::prepare:    //could not prepare a SQL statement -> (fatal) terminate server
                case server::DatabaseError_pwd::finalize:   //could not finalize SQL statement -> (fatal) terminate server
                case server::DatabaseError_pwd::insert: //could not insert into the database -> (fatal) terminate server (not used here)
                case server::DatabaseError_pwd::update: //could not update into the database -> (fatal) terminate server (not used here)
                case server::DatabaseError_pwd::remove: //could not remove from the database -> (fatal) terminate server (not used here)

                //this is the only PWT_Database error that could happen in the server threads
                case server::DatabaseError_pwd::read:   //could not get hash,salt pair from the database -> (fatal) terminate server (not used here)
                default:
                    //Fatal error -> close the server
                    Message::print(std::cerr, "ERROR", "PWD_Database Exception", e.what());

                    server_stop.store(true);    //set the stop atomic boolean for the main (the main will stop all the other threads)

                    Socket tmp{SocketType::TCP};
                    tmp.connect("localhost",PORT); //connect to the local serverSocket in order to make it exit the accept

                    return; //then return
            }
        }
        catch (server::DatabaseException &e) {
            switch(e.getCode()){
                case server::DatabaseError::create: //could not create the table in the database -> (fatal) terminate server
                case server::DatabaseError::open:   //could not open the database -> (fatal) terminate server
                case server::DatabaseError::prepare:    //could not prepare a SQL statement -> (fatal) terminate server
                case server::DatabaseError::finalize:   //could not finalize SQL statement -> (fatal) terminate server
                case server::DatabaseError::insert: //could not insert into the database -> (fatal) terminate server (not used here)
                case server::DatabaseError::read:   //could not read from the database -> (fatal) terminate server (not used here)
                case server::DatabaseError::update: //could not update into the database -> (fatal) terminate server (not used here)
                case server::DatabaseError::remove: //could not remove from the database -> (fatal) terminate server (not used here)
                default:
                    //Fatal error -> close the server
                    Message::print(std::cerr, "ERROR", "Database Exception", e.what());

                    server_stop.store(true);    //set the stop atomic boolean for the main (the main will stop all the other threads)

                    Socket tmp{SocketType::TCP};
                    tmp.connect("localhost",PORT); //connect to the local serverSocket in order to make it exit the accept

                    return; //then return
            }
        }
        catch (server::ConfigException &e) {
            switch(e.getCode()){
                case server::ConfigError::justCreated:  //if the config file was not there and it has been created
                case server::ConfigError::serverBasePath:  //if the configured server base path was not specified or it does not exist (or it is not a directory) ask to modify it and return
                case server::ConfigError::tempPath: //if the configured server temporary path was not specified ask to modify it and return
                    Message::print(std::cout, "ERROR", "Please check config file: ", CONFIG_FILE_PATH);

                case server::ConfigError::open: //if there were some errors in opening the configuration file return
                default:
                    Message::print(std::cerr, "ERROR", "Config Exception", e.what());

                    server_stop.store(true);    //set the stop atomic boolean for the main (the main will stop all the other threads)

                    Socket tmp{SocketType::TCP};
                    tmp.connect("localhost",PORT); //connect to the local serverSocket in order to make it exit the accept

                    return; //then return
            }
        }
        catch (std::exception &e) {
            //Fatal error -> close the server
            Message::print(std::cerr, "ERROR", "exception", e.what());

            server_stop.store(true);    //set the stop atomic boolean for the main (the main will stop all the other threads)

            Socket tmp{SocketType::TCP};
            tmp.connect("localhost",PORT); //connect to the local serverSocket in order to make it exit the accept

            return; //then return
        }
    }
}