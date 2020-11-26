//
// Created by michele on 25/07/2020.
//

#include <chrono>
#include <string>
#include <iostream>
#include <atomic>
#include <getopt.h>
#include <regex>
#include "FileSystemWatcher.h"
#include "Event.h"
#include "../myLibraries/Circular_vector.h"
#include "Thread_guard.h"
#include "../myLibraries/Socket.h"
#include "messages.pb.h"
#include "ProtocolManager.h"
#include "../myLibraries/Message.h"
#include "../myLibraries/Validator.h"
#include "Config.h"

//TODO check

#define VERSION 1

#define SOCKET_TYPE SocketType::TLS
#define CONFIG_FILE_PATH "../config.txt"

void communicate(std::atomic<bool> &, std::atomic<bool> &, TS_Circular_vector<Event> &, const std::string &, int, const std::string &, const std::string &);

void displayHelp(const std::string &programName){
    std::cout << "\nNAME" << std::endl << "\t";
    std::cout << "PDS_BACKUP client\n" << std::endl;
    std::cout << "SYNOPSIS" << std::endl << "\t";
    std::cout  << programName << " [--help] [--retrieve destFolder] [--mac macAddress] [--all] [--start] [--ip server_ipaddress] [--port server_port] [--user username] [--pass password]\n" << std::endl;
    std::cout << "OPTIONS" << std::endl << "\t";
    std::cout << "--help (abbr -h)" << std::endl << "\t\t";
    std::cout << "Print out a usage message\n" << std::endl << "\t";
    std::cout << "--retrieve (abbr -r)" << std::endl << "\t\t";
    std::cout << "Requests the server (after authentication) to send to the client the copy of the folders and files of\n\t\t"
                 "the specified user. The data will be put in the specified [destDir]. If no other commands are specified\n\t\t"
                 "(no --mac, no --all) then only the files and directories for the current mac address will be retrieved.\n\t\t"
                 "This command requires the presence of the following other commands: [--ip] [--port] [--user] [--pass] [--dir]\n" << std::endl << "\t";
    std::cout << "--dir (abbr -m) destDir" << std::endl << "\t\t";
    std::cout << "Sets the [destDir] of the user's data to retrieve.\n\t\t"
                 "Needed by --retrieve.\n" << std::endl << "\t";
    std::cout << "--mac (abbr -m) macAddress" << std::endl << "\t\t";
    std::cout << "Sets the [macAddress] of the user's data to retrieve.\n\t\t"
                 "To be used with --retrieve.\n" << std::endl << "\t";
    std::cout << "--all (abbr -a)" << std::endl << "\t\t";
    std::cout << "Specifies to retrieve all user's data,\n\t\t"
                 "To be used with --retrieve.\n" << std::endl << "\t";
    std::cout << "--start (abbr -s)" << std::endl << "\t\t";
    std::cout << "Start the server (if not present the server will stop after having created/loaded the Config file).\n\t\t"
                 "This command requires the presence of the following other commands: [--ip] [--port] [--user] [--pass]\n" << std::endl << "\t";
    std::cout << "--ip (abbr -i) server_ipaddress" << std::endl << "\t\t";
    std::cout << "Sets the [ip] address of the server to contact.\n\t\t"
                 "Needed by --start and --retrieve.\n" << std::endl << "\t";
    std::cout << "--port (abbr -p) server_port" << std::endl << "\t\t";
    std::cout << "Sets the [port] of the server to contact.\n\t\t"
                 "Needed by --start and --retrieve.\n" << std::endl << "\t";
    std::cout << "--user (abbr -u) username" << std::endl << "\t\t";
    std::cout << "Sets the [username] to use to authenticate to the server.\n\t\t"
                 "Needed by --start and --retrieve.\n" << std::endl << "\t";
    std::cout << "--pass (abbr -w) password" << std::endl << "\t\t";
    std::cout << "Sets the [password] to use to authenticate to the server.\n\t\t"
                 "Needed by --start and --retrieve.\n" << std::endl;
}

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

    //--options management--
    std::string destFolder, mac, serverIP, serverPort, username, password;
    bool retrieveSet = false, dirSet = false, macSet = false, allSet = false, startSet = false, ipSet = false, portSet = false, userSet = false, passSet = false;

    if(argc == 1){
        std::cout << "Options expected. Use -h (or --help) for help." << std::endl;
        return 1;
    }

    int c;
    while (true) {
        int option_index = 0;
        static struct option long_options[] = { //long options definition
                {"retrieve",    no_argument,        nullptr,  'r' },
                {"dir",         required_argument,  nullptr,  'd' },
                {"mac",         required_argument,  nullptr,  'm' },
                {"all",         no_argument,        nullptr,  'a' },
                {"start",       no_argument,        nullptr,  's' },
                {"ip",          required_argument,  nullptr,  'i' },
                {"port",        required_argument,  nullptr,  'p' },
                {"username",    required_argument,  nullptr,  'u' },
                {"password",    required_argument,  nullptr,  'w' },
                {"help",        no_argument,        nullptr,  'h'},
                {nullptr,0,                 nullptr,  0 }
        };

        c = getopt_long(argc, argv, "rd:m:asi:p:u:w:h", long_options, &option_index);     //short options definition and the getting of an option
        if (c == -1)
            break;

        //if you insert a command which requires an argument, but then forget to actually insert the argument:
        //  if the command was at the end of the whole command string then the getopt function will realize it and signal an error
        //  if the command was followed by another command the second command will be interpreted as the first command's argument

        //so to mitigate that check if the optional argument is actually valid (it must not be a command, so it must have no heading '-')
        if(optarg != nullptr && !Validator::validateOptArg(optarg)) {
            Message::print(std::cerr, "ERROR", "Error with an option argument inserted",
                           "Maybe you forgot one argument");
            return 1;
        }

        switch (c) {
            case 'r':   //retrieve option
                retrieveSet = true;
                break;

            case 'd':   //dir option
                dirSet = true;
                destFolder = optarg;

                //validate folder
                if(!Validator::validatePath(destFolder)) {
                    Message::print(std::cerr, "ERROR", "Error with the destination folder inserted",
                                   "Insert a valid folder");
                    return 1;
                }

                break;

            case 'm':   //mac option
                macSet = true;
                mac = optarg;

                //validate mac
                if(!Validator::validateMacAddress(mac)) {
                    Message::print(std::cerr, "ERROR", "Error with the mac inserted",
                                   "Insert a valid mac");
                    return 1;
                }

                break;

            case 'a':   //all option
                allSet = true;
                break;

            case 's':   //start client option
                startSet = true;
                break;

            case 'i':   //ip option
                ipSet = true;
                serverIP = optarg;

                //validate ip address
                if(!Validator::validateIPAddress(serverIP)) {
                    Message::print(std::cerr, "ERROR", "Error with the IP address inserted",
                                   "Insert a valid IP address");
                    return 1;
                }

                break;

            case 'p':   //port option
                portSet = true;
                serverPort = optarg;

                //validate port
                if(!Validator::validatePort(serverPort)) {
                    Message::print(std::cerr, "ERROR", "Error with the server port inserted",
                                   "Insert a valid port");
                    return 1;
                }

                break;

            case 'u':   //username option
                userSet = true;
                username = optarg;

                //validate username
                if(!Validator::validateUsername(username)) {
                    Message::print(std::cerr, "ERROR", "Error with the username inserted",
                                   "Insert a valid username string");
                    return 1;
                }

                break;

            case 'w':   //password option
                passSet = true;
                password = optarg;

                //validate password
                if(!Validator::validatePassword(password)) {
                    Message::print(std::cerr, "ERROR", "Error with the password inserted",
                                   "Insert a valid password string");
                    return 1;
                }

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

    if (optind >= argc) {   //if last option requires an argument but none was provided
        std::regex e(R"(^(?:(?:-[dmipuw])|(?:--(?:(?:dir)|(?:mac)|(?:ip)|(?:port)|(?:username)|(?:password))))$)");   //matches all the options which require an extra argument
        std::smatch m;

        std::string lastArgument = argv[optind-1];  //get last argument

        if(std::regex_match(lastArgument, m, e)) {  //check if the last argument is actually an argument requesting option
            Message::print(std::cerr, "ERROR", "Error with the arguments", "Expected argument after options");
            return 1;
        }
    }

    //perform some checks on the options
    if(!retrieveSet && (macSet || allSet || dirSet)){
        Message::print(std::cerr, "ERROR", "--mac, --all and --dir options require --retrieve.", "Use -h (or --help) for help.");
        return 1;
    }

    if(retrieveSet && (!ipSet || !portSet || !userSet || !passSet || !dirSet)){
        Message::print(std::cerr, "ERROR", "--retrieve command requires --ip --port --user --pass --dir options to be set.", "Use -h (or --help) for help.");
        return 1;
    }

    if(startSet && (!ipSet || !portSet || !userSet || !passSet)){
        Message::print(std::cerr, "ERROR", "--start command requires --ip --port --user --pass options to be set.", "Use -h (or --help) for help.");
        return 1;
    }

    if(!startSet && !retrieveSet){
        Message::print(std::cerr, "ERROR", "--start AND/OR --retrieve options need to be specified", "Use -h (or --help) for help.");
        return 1;
    }

    try {
        //get the configuration
        client::Config::setPath(CONFIG_FILE_PATH);
        auto config = client::Config::getInstance();
        //get the database instance and open the database (and if not previously there create also the needed table)
        client::Database::setPath(config->getDatabasePath());
        auto db = client::Database::getInstance();

        if(retrieveSet){
            //specfy the TLS certificates for the socket
            Socket::specifyCertificates(config->getCAFilePath());
            //create the socket
            Socket client_socket{SOCKET_TYPE};

            //connect to the server through the socket
            client_socket.connect(serverIP, stoi(serverPort));

            //create the protocol manager instance
            client::ProtocolManager pm(client_socket, VERSION);

            //authenticate the client to the server
            pm.authenticate(username, password, client_socket.getMAC());

            //send the RETR message and get all the data from server
            if(macSet){
                Message::print(std::cout, "INFO", "Will retrieve all your files corresponding to mac: " + mac, "destination folder: " + destFolder);

                //send RETR message with the set mac and get all data from server and save it in destFolder
                pm.retrieveFiles(mac, false, destFolder);
            }
            else if(allSet){
                Message::print(std::cout, "INFO", "Will retrieve all your files", "destination folder: " + destFolder);

                //send RETR message with all set and get all data from server and save it in destFolder
                pm.retrieveFiles("", true, destFolder);
            }
            else{
                std::string thisSocketMac = client_socket.getMAC();
                Message::print(std::cout, "INFO", "Will retrieve all your files corresponding to mac: " + thisSocketMac, "destination folder: " + destFolder);

                //send RETR message with this machine's mac address and get all data from server and save it in destFolder
                pm.retrieveFiles(thisSocketMac, false, destFolder);
            }
        }

        if(!startSet)   //if the --start option was not there just close the program
            return 0;

        // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
        FileSystemWatcher fw{config->getPathToWatch(), std::chrono::milliseconds(config->getMillisFilesystemWatcher())};

        // create a circular vector instance that will contain all the events happened
        TS_Circular_vector<Event> eventQueue(config->getEventQueueSize());

        //initialize some atomic boolean to make the different threads stop
        std::atomic<bool> communicationThread_stop = false, fileWatcher_stop = false;

        // initialize the communication thread (thread that will communicate with the server)
        std::thread communication_thread(communicate, std::ref(communicationThread_stop), std::ref(fileWatcher_stop), std::ref(eventQueue), serverIP, stoi(serverPort), username, password);

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