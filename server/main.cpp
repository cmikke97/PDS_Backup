#include <iostream>
#include <messages.pb.h>
#include <sstream>
#include <thread>
#include <getopt.h>
#include "../myLibraries/Socket.h"
#include "../myLibraries/TSCircular_vector.h"
#include "Thread_guard.h"
#include "PWD_Database.h"
#include "Database.h"
#include "ProtocolManager.h"

#define VERSION 1
#define CONFIG_FILE_PATH "./config.txt"

#define LISTEN_QUEUE 8
#define N_THREADS 8
#define SOCKET_QUEUE_SIZE 10
#define SELECT_TIMEOUT_SECONDS 5
#define TIMEOUT_SECONDS 60
#define PASSWORD_DATABASE_PATH "C:/Users/michele/CLionProjects/PDS_Backup/server/serverfiles/passwordDB.sqlite"
#define DATABASE_PATH "C:/Users/michele/CLionProjects/PDS_Backup/server/serverfiles/serverDB.sqlite"
#define SERVER_PATH "C:/Users/michele/Desktop/server_folder"

#define PORT 8081
//TODO put these constants into the config object

void single_server(TSCircular_vector<Socket> &, std::atomic<bool> &);

void displayHelp(const std::string &programName){
    std::cout << "\nNAME" << std::endl << "\t";
    std::cout << "PDS_BACKUP server\n" << std::endl;
    std::cout << "SYNOPSIS" << std::endl << "\t";
    std::cout  << programName << " [--help] [--addU username] [--updateU username] [--removeU username] [--pass password] [--start]\n" << std::endl;
    std::cout << "OPTIONS" << std::endl << "\t";
    std::cout << "--help (abbr -h)" << std::endl << "\t\t";
    std::cout << "Print out a usage message\n" << std::endl << "\t";
    std::cout << "--addU (abbr -a) username" << std::endl << "\t\t";
    std::cout << "Add the user with [username] to the server, the option --pass (-p) is needed to set the user password.\n\t\t"
                 "This option is mutually exclusive with --updateU and --removeU.\n" << std::endl << "\t";
    std::cout << "--updateU (abbr -u) username" << std::endl << "\t\t";
    std::cout << "Update the user with [username] to the server, the option --pass (-p) is needed to set the new user password.\n\t\t"
                 "This option is mutually exclusive with --addU and --removeU.\n" << std::endl << "\t";
    std::cout << "--removeU (abbr -r) username" << std::endl << "\t\t";
    std::cout << "Remove the user with [username] from the server.\n\t\t"
                 "This option is mutually exclusive with --addU and --removeU.\n" << std::endl << "\t";
    std::cout << "--pass (abbr -p) password" << std::endl << "\t\t";
    std::cout << "Set the [password] to use.\n\t\t"
                 "This option is needed by the options --addU and --updateU.\n" << std::endl << "\t";
    std::cout << "--start (abbr -s)" << std::endl << "\t\t";
    std::cout << "Start the server" << std::endl;
}

int main(int argc, char** argv) {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    //--options management--
    std::string username, password;
    bool addU = false, updateU = false, removeU = false, viewU = false, passSet = false, start = false;

    if(argc == 1){
        std::cout << "Options expected. Use -h (or --help) for help." << std::endl;
        return 1;
    }

    int c;
    while (true) {
        int option_index = 0;
        static struct option long_options[] = { //long options definition
                {"addU",     required_argument, nullptr,  'a' },
                {"updateU",  required_argument, nullptr,  'u' },
                {"removeU",  required_argument, nullptr,  'r' },
                {"viewU",    no_argument,       nullptr,  'v' },
                {"pass",     required_argument, nullptr,  'p' },
                {"start",    no_argument,       nullptr,  's' },
                {"help",     no_argument,       nullptr,  'h'},
                {nullptr,    0,         nullptr,  0 }
        };

        c = getopt_long(argc, argv, "a:u:r:vp:sh", long_options, &option_index);     //short options definition and the getting of an option
        if (c == -1)
            break;

        switch (c) {
            case 'a':   //add user option
                addU = true;
                username = optarg;
                break;

            case 'u':   //update user option
                updateU = true;
                username = optarg;
                break;

            case 'r':   //remove user option
                removeU = true;
                username = optarg;
                break;

            case 'v':   //view all users option
                viewU = true;
                break;

            case 'p':   //password option
                passSet = true;
                password = optarg;
                break;

            case 's':   //start server option
                start = true;
                break;

            case 'h':   //help option
                displayHelp(argv[0]);
                return 0;

            case '?':   //unknown option
                break;

            default:    //error from getopt
                std::cerr << "?? getopt returned character code 0" << c << " ??" << std::endl;
        }
    }

    //some checks on the options

    if((addU && updateU) || (addU && removeU) || (updateU && removeU)){ //only one of these should be true (they are mutually exclusive)
        std::cerr << "Mutual exclusive options. Use -h (or --help) for help." << std::endl;
        return 1;
    }

    if((addU && !passSet) || (updateU && !passSet)){    //if addU/updateU is active then I need the password
        std::cerr << "Password option needed. Use -h (or --help) for help." << std::endl;
        return 1;
    }

    try{
        //TODO get config
        auto pass_db = server::PWD_Database::getInstance(PASSWORD_DATABASE_PATH);
        auto db = server::Database::getInstance(DATABASE_PATH);

        if(addU){   //add user to the server
            pass_db->addUser(username, password);
            std::cout << "User " << username << " added to server." << std::endl;
        }

        if(updateU){    //update user in the server
            pass_db->updateUser(username, password);
            std::cout << "User " << username << " updated on server." << std::endl;
        }

        if(removeU){    //remove user from the server
            pass_db->removeUser(username);
            std::cout << "User " << username << " removed from server." << std::endl;
        }

        if(viewU){  //view all users registered in the server
            std::cout << "Registered Users:" << std::endl;
            std::function<void (const std::string &)> f = [](const std::string &username){
                std::cout << "\t" << username << std::endl;
            };
            pass_db->forAll(f);
        }

        if(start) { //start the server
            std::cout << "Starting service.." << std::endl;
            ServerSocket server{PORT, LISTEN_QUEUE, socketType::TCP};   //initialize server socket with port
            TSCircular_vector<Socket> sockets{SOCKET_QUEUE_SIZE};

            std::atomic<bool> server_thread_stop = false;   //atomic boolean to force the server threads to stop
            std::vector<std::thread> threads;

            for (int i = 0; i < N_THREADS; i++)  //create N_THREADS threads and start them
                threads.emplace_back(single_server, std::ref(sockets), std::ref(server_thread_stop));

            Thread_guard td{threads, server_thread_stop};

            struct sockaddr_in addr{};
            unsigned long addr_len = sizeof(addr);

            while (true) {
                //alternative: wait here for free space in circular vector and then accept and push

                Socket s = server.accept(&addr, &addr_len); //accept a new client socket
                sockets.push(std::move(s)); //push the socket into the socket queue

                auto clientAddress = inet_ntoa(addr.sin_addr);  //obtain ip address of newly connected client
                std::cout << "New connection from " << clientAddress << ":" << addr.sin_port
                          << std::endl;  //print ip address and port of the newly connected client
            }
        }
    }
    catch (SocketException &e) {
        switch(e.getCode()){
            case socketError::create:
            case socketError::accept:
            default:
                std::cerr << e.what() << std::endl;
                return 1;
        }
    }
    catch (server::PWT_DatabaseException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (server::DatabaseException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}

void single_server(TSCircular_vector<Socket> &sockets, std::atomic<bool> &thread_stop){
    while(!thread_stop.load()){ //loop until we are told to stop
        Socket sock = sockets.get();    //get the first socket in the socket queue (removing it from the queue); if no element is present then passively wait

        //for select
        fd_set read_fds;
        int timeWaited = 0;
        bool loop = true;
        server::ProtocolManager pm{sock, VERSION, SERVER_PATH};

        try{
            //authenticate the connected client
            pm.authenticate();
            pm.recoverFromDB();

            while(loop && !thread_stop.load()) { //loop until we are told to stop
                //build fd sets
                FD_ZERO(&read_fds);
                FD_SET(sock.getSockfd(), &read_fds);

                int maxfd = sock.getSockfd();
                struct timeval tv{};
                tv.tv_sec = SELECT_TIMEOUT_SECONDS;

                //select on read socket
                int activity = select(maxfd + 1, &read_fds, nullptr, nullptr, &tv);

                switch (activity) {
                    case -1:
                        //I should never get here
                        std::cerr << "Select error" << std::endl;

                        loop = false;      //exit loop --> this will cause the current connection to be closed
                        break;

                    case 0:
                        timeWaited += SELECT_TIMEOUT_SECONDS;

                        if (timeWaited >= TIMEOUT_SECONDS)   //if the time already waited is greater than TIMEOUT
                            loop = false;      //then exit loop --> this will cause the current connection to be closed

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
        catch (SocketException &e) {
            switch(e.getCode()){
                case socketError::create:
                case socketError::accept:
                default:
                    std::cerr << e.what() << std::endl;
                    continue;
            }
        }
        catch (server::PWT_DatabaseException &e) {
            std::cerr << e.what() << std::endl;
            continue;
        }
        catch (server::DatabaseException &e) {
            std::cerr << e.what() << std::endl;
            continue;
        }
        catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
            continue;
        }
        //TODO authenticate client

        //TODO while loop (communication)
            //TODO select -> read/write(only for async)
                /*TODO if there is something to read pass it to the protocol manager (pm);
                 * (sync alternative): the protocol manager reads the message, interprets it and based on the type
                 * it performs the related actions; when all actions are done it sends the response to the client
                 * (async alternative)L the protocol manager reads the message and inputs it in a list of incoming
                 * messages; a concurrent thread gets messages from the list of incoming messages, interprets them
                 * and based on the types it performs the related actions; when all actions related to a single message
                 * are done it inputs the related response message in the list of outgoing messages; so now if there
                 * are messages in the list of outgoing messages then send one of them (for every select cycle)*/

        //TODO implement protocol manager
    }
}