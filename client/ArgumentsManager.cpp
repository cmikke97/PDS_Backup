//
// Created by Michele Crepaldi s269551 on 27/11/2020
// Finished on 27/11/2020
// Last checked on 27/11/2020
//

#include "ArgumentsManager.h"

#include <iostream>
#include <getopt.h>
#include <regex>

#include "../myLibraries/Validator.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * ArgumentsManager class methods
 */

/**
 * ArgumentsManager constructor.
 *  Used to retrieve (and set) all the options (and their option arguments) from the arguments from main.
 *  It also validates the option arguments.
 *
 * @param argc argc from main
 * @param argv argv from main
 *
 * @throws ArgumentsManagerException:
 *  <b>numberOfArguments</b> if the number of arguments is wrong (it must be >1)
 * @throws ArgumentsManagerException:
 *  <b>option</b> if an option is missing its option argument, or if there was an error with getopt function
 * @throws ArgumentsManagerException:
 *  <b>optArgument</b> if there was an error with an option argument
 * @throws ArgumentsManagerException:
 *  <b>help</b> if the help option was set
 *
 * @author Michele Crepaldi s269551
 */
client::ArgumentsManager::ArgumentsManager(int argc, char **argv) : //pre-initialize all the booleans to false
        _userSet(false),
        _passSet(false),
        _retrSet(false),
        _dirSet(false),
        _macSet(false),
        _allSet(false),
        _startSet(false),
        _ipSet(false),
        _portSet(false){

    //if the number of arguments is wrong throw exception
    if(argc == 1)
        throw ArgumentsManagerException("Options expected. Use -h (or --help) for help.",
                                        ArgumentsManagerError::numberOfArguments);

    int c;  //current argument value
    while (true) {
        int option_index = 0;   //current option index

        static struct option long_options[] = { //supported long options definition
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

        //define short (+long) options and get next option from the arguments from main
        c = getopt_long(argc, argv, "rd:m:asi:p:u:w:h", long_options, &option_index);

        //if no more options were found then exit loop
        if (c == -1)
            break;

        //if you insert an option which requires an argument, but then forget to actually insert the argument:

        //if the option was at the end of the whole argument string then the getopt function will realize
        //it and signal an error

        //if the option was followed by another option the second option will be interpreted as the first
        //option's argument

        //so to solve that check if the optional argument is actually valid
        //(it must not be an option, so it must have no heading '-')
        if(optarg != nullptr && !Validator::validateOptArg(optarg))
            throw ArgumentsManagerException("Error with an option inserted. Maybe you forgot one option argument",
                                            ArgumentsManagerError::option);

        //switch on the argument value
        switch (c) {
            case 'r':   //retrieve option
                _retrSet = true;
                break;

            case 'd':   //dir option
                _dirSet = true;
                _destFolder = optarg;

                //validate folder
                if(!Validator::validatePath(_destFolder))
                    throw ArgumentsManagerException("Error with the destination folder inserted. Insert a valid folder",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'm':   //mac option
                _macSet = true;
                _mac = optarg;

                //validate mac
                if(!Validator::validateMacAddress(_mac))
                    throw ArgumentsManagerException("Error with the mac inserted. Insert a valid mac address",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'a':   //all option
                _allSet = true;
                break;

            case 's':   //start client option
                _startSet = true;
                break;

            case 'i':   //ip option
                _ipSet = true;
                _serverIp = optarg;

                //validate ip address
                if(!Validator::validateIPAddress(_serverIp))
                    throw ArgumentsManagerException("Error with the IP address inserted. Insert a valid IP address",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'p':   //port option
                _portSet = true;
                _serverPort = optarg;

                //validate port
                if(!Validator::validatePort(_serverPort))
                    throw ArgumentsManagerException("Error with the server port inserted. Insert a valid port",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'u':   //username option
                _userSet = true;
                _username = optarg;

                //validate username
                if(!Validator::validateUsername(_username))
                    throw ArgumentsManagerException("Error with the username inserted. Insert a valid username string",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'w':   //password option
                _passSet = true;
                _password = optarg;

                //validate password
                if(!Validator::validatePassword(_password))
                    throw ArgumentsManagerException("Error with the password inserted. Insert a valid password string",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'h':   //display help option
                _displayHelp(argv[0]);
                throw ArgumentsManagerException("", ArgumentsManagerError::help);

            case '?':   //unknown option
                break;

            default:    //error from getopt
                std::cerr << "?? getopt returned character code 0" << c << " ??" << std::endl;
                throw ArgumentsManagerException("",ArgumentsManagerError::option);
        }
    }

    //if last option requires an argument but none was provided
    if (optind >= argc) {
        //matches all the options which require an extra argument
        std::regex e(R"(^(?:(?:-[dmipuw])|(?:--(?:(?:dir)|(?:mac)|(?:ip)|(?:port)|(?:username)|(?:password))))$)");
        std::smatch m;

        std::string lastArgument = argv[optind-1];  //last argument inserted

        //check if the last argument is actually an argument requesting option
        if(std::regex_match(lastArgument, m, e))
            throw ArgumentsManagerException("Error with an option inserted. Maybe you forgot one option argument",
                                            ArgumentsManagerError::option);
    }

    //perform some checks on the options

    //the mac, all and dir options need the retrieve option to be set
    if(!_retrSet && (_macSet || _allSet || _dirSet))
        throw ArgumentsManagerException("--mac, --all and --dir options require --retrieve."
                                        " Use -h (or --help) for help.",
                                        ArgumentsManagerError::optArgument);

    //retrieve option requires ip, port, user, pass and dir options to be set
    if(_retrSet && (!_ipSet || !_portSet || !_userSet || !_passSet || !_dirSet))
        throw ArgumentsManagerException("--retrieve command requires --ip --port --user --pass --dir options"
                                        " to be set. Use -h (or --help) for help.",
                                        ArgumentsManagerError::optArgument);

    //start option requires ip, port, user and pass options to be set
    if(_startSet && (!_ipSet || !_portSet || !_userSet || !_passSet))
        throw ArgumentsManagerException("--start command requires --ip --port --user --pass options to be set. Use -h (or --help) for help.",
                                        ArgumentsManagerError::optArgument);

    //start/retrieve options are required
    if(!_startSet && !_retrSet)
        throw ArgumentsManagerException("--start AND/OR --retrieve options need to be specified. Use -h (or --help) for help.",
                                        ArgumentsManagerError::optArgument);
}

/**
 * ArgumentsManager displayHelp method.
 *  Used to print a help menu on standard output.
 *
 * @param programName name of the program
 *
 * @author Michele Crepaldi s269551
 */
void client::ArgumentsManager::_displayHelp(const std::string &programName){
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
 * ArgumentsManager username option argument getter method.
 *
 * @return username option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &client::ArgumentsManager::getUsername() const {
    return _username;
}

/**
 * ArgumentsManager password option argument getter method.
 *
 * @return password option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &client::ArgumentsManager::getPassword() const {
    return _password;
}

/**
 * ArgumentsManager mac address option argument getter method.
 *
 * @return mac address option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &client::ArgumentsManager::getMac() const {
    return _mac;
}

/**
 * ArgumentsManager destination folder option argument getter method.
 *
 * @return destination folder option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &client::ArgumentsManager::getDestFolder() const {
    return _destFolder;
}

/**
 * ArgumentsManager server ip option argument getter method.
 *
 * @return server ip option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &client::ArgumentsManager::getServerIp() const {
    return _serverIp;
}

/**
 * ArgumentsManager server port option argument getter method.
 *
 * @return server port option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &client::ArgumentsManager::getSeverPort() const {
    return _serverPort;
}

/**
 * ArgumentsManager isUserSet option getter.
 *
 * @return whether the user option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isUserSet() const {
    return _userSet;
}

/**
 * ArgumentsManager isPassSet option getter.
 *
 * @return whether the pass option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isPassSet() const {
    return _passSet;
}

/**
 * ArgumentsManager isRetrSet option getter.
 *
 * @return whether the retr option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isRetrSet() const {
    return _retrSet;
}

/**
 * ArgumentsManager isDirSet option getter.
 *
 * @return whether the dir option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isDirSet() const {
    return _dirSet;
}

/**
 * ArgumentsManager isMacSet option getter.
 *
 * @return whether the mac option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isMacSet() const {
    return _macSet;
}

/**
 * ArgumentsManager isAllSet option getter.
 *
 * @return whether the all option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isAllSet() const {
    return _allSet;
}

/**
 * ArgumentsManager isStartSet option getter.
 *
 * @return whether the start option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isStartSet() const {
    return _startSet;
}

/**
 * ArgumentsManager isIpSet option getter.
 *
 * @return whether the ip option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isIpSet() const {
    return _ipSet;
}

/**
 * ArgumentsManager isPortSet option getter.
 *
 * @return whether the port option was set
 *
 * @author Michele Crepaldi s269551
 */
bool client::ArgumentsManager::isPortSet() const {
    return _portSet;
}
