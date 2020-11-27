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
server::ArgumentsManager::ArgumentsManager(int argc, char **argv) : //pre-initialize all the booleans to false
        _addSet(false),
        _deleteSet(false),
        _macSet(false),
        _passSet(false),
        _removeSet(false),
        _startSet(false),
        _updateSet(false),
        _viewSet(false) {

    //if the number of arguments is wrong throw exception
    if(argc == 1)
        throw ArgumentsManagerException("Options expected. Use -h (or --help) for help.",
                                        ArgumentsManagerError::numberOfArguments);

    int c;  //current argument value
    while (true) {
        int option_index = 0;   //current option index

        static struct option long_options[] = { //supported long options definition
                {"addU",     required_argument, nullptr,  'a' },
                {"updateU",  required_argument, nullptr,  'u' },
                {"removeU",  required_argument, nullptr,  'r' },
                {"viewU",    no_argument,       nullptr,  'v' },
                {"pass",     required_argument, nullptr,  'p' },
                {"delete",   required_argument, nullptr,  'd' },
                {"mac",      required_argument, nullptr,  'm' },
                {"start",    no_argument,       nullptr,  's' },
                {"help",     no_argument,       nullptr,  'h'},
                {nullptr,    0,         nullptr,  0 }
        };

        //define short (+long) options and get next option from the arguments from main
        c = getopt_long(argc, argv, "a:u:r:vp:d:m:sh", long_options, &option_index);

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
            case 'a':   //add user option
                _addSet = true;
                _username = optarg;

                //validate username
                if(!Validator::validateUsername(_username))
                    throw ArgumentsManagerException("Error with the username inserted. Insert a valid username string",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'u':   //update user option
                _updateSet = true;
                _username = optarg;

                //validate username
                if(!Validator::validateUsername(_username))
                    throw ArgumentsManagerException("Error with the username inserted. Insert a valid username string",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'r':   //remove user option
                _removeSet = true;
                _username = optarg;

                //validate username
                if(!Validator::validateUsername(_username))
                    throw ArgumentsManagerException("Error with the username inserted. Insert a valid username string",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'v':   //view all users option
                _viewSet = true;
                break;

            case 'p':   //password option
                _passSet = true;
                _password = optarg;

                //validate password
                if(!Validator::validatePassword(_password))
                    throw ArgumentsManagerException("Error with the password inserted. Insert a valid password string",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'd':   //delete user option
                _deleteSet = true;
                _delUsername = optarg;

                //validate username
                if(!Validator::validateUsername(_delUsername))
                    throw ArgumentsManagerException("Error with the (del) username inserted. Insert a valid username string",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 'm':   //mac (coupled with delete option) option
                _macSet = true;
                _delMac = optarg;

                //validate mac
                if(!Validator::validateMacAddress(_delMac))
                    throw ArgumentsManagerException("Error with the mac inserted. Insert a valid mac address",
                                                    ArgumentsManagerError::optArgument);

                break;

            case 's':   //start server option
                _startSet = true;
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
        std::regex e(R"(^(?:(?:-[aurpdm])|(?:--(?:(?:addU)|(?:updateU)|(?:removeU)|(?:pass)|(?:delete)|(?:mac))))$)");
        std::smatch m;

        std::string lastArgument = argv[optind-1];  //last argument inserted

        //check if the last argument is actually an argument requesting option
        if(std::regex_match(lastArgument, m, e))
            throw ArgumentsManagerException("Error with an option inserted. Maybe you forgot one option argument",
                                            ArgumentsManagerError::option);
    }

    //perform some checks on the options

    //only one of these options should be set (they are mutually exclusive)
    if((_addSet && _updateSet) || (_addSet && _removeSet) || (_updateSet && _removeSet))
        throw ArgumentsManagerException("Mutual exclusive options set. Use -h (or --help) for help.",
                                        ArgumentsManagerError::optArgument);

    //if add or update options are active then I need the password
    if((_addSet && !_passSet) || (_updateSet && !_passSet))
        throw ArgumentsManagerException("Password option needed. Use -h (or --help) for help.",
                                        ArgumentsManagerError::optArgument);

    //the mac option need the delete option to be set
    if(_macSet && !_deleteSet)
        throw ArgumentsManagerException("--mac option requires --delete option. Use -h (or --help) for help.",
                                        ArgumentsManagerError::optArgument);
}

/**
 * ArgumentsManager diplayHelp method.
 *  Used to print a help menu on standard output.
 *
 * @param programName name of the program
 *
 * @author Michele Crepaldi s269551
 */
void server::ArgumentsManager::_displayHelp(const std::string &programName){
    std::cout << "\nNAME" << std::endl << "\t";
    std::cout << "PDS_BACKUP server\n" << std::endl;
    std::cout << "SYNOPSIS" << std::endl << "\t";
    std::cout  << programName << " [--help] [--addU username] [--updateU username] [--removeU username] [--viewU] [--pass password] [--delete username] [--mac macAddress] [--start]\n" << std::endl;
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
    std::cout << "--viewU (abbr -v)" << std::endl << "\t\t";
    std::cout << "Print all the username of all registered users.\n" << std::endl << "\t";
    std::cout << "--pass (abbr -p) password" << std::endl << "\t\t";
    std::cout << "Set the [password] to use.\n\t\t"
                 "This option is needed by the options --addU and --updateU.\n" << std::endl << "\t";
    std::cout << "--delete (abbr -d) username" << std::endl << "\t\t";
    std::cout << "Makes the server delete all or some of the specified [username] backups before (optionally) starting the service.\n\t\t"
                 "If no other options (no --mac) are specified then it will remove all the user's backups from server.\n" << std::endl << "\t";
    std::cout << "--mac (abbr -m) macAddress" << std::endl << "\t\t";
    std::cout << "Specifies the [macAddress] for the --delete option.\n\t\t"
                 "If this option is set the --delete option will only delete the user's backup related to this [macAddress].\n" << std::endl << "\t";
    std::cout << "--start (abbr -s)" << std::endl << "\t\t";
    std::cout << "Start the server" << std::endl;
}

/**
 * ArgumentsManager username option argument getter method.
 *
 * @return username option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::ArgumentsManager::getUsername() const {
    return _username;
}

/**
 * ArgumentsManager password option argument getter method.
 *
 * @return password option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::ArgumentsManager::getPassword() const {
    return _password;
}

/**
 * ArgumentsManager delUsername option argument getter method.
 *
 * @return delUsername option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::ArgumentsManager::getDelUsername() const {
    return _delUsername;
}

/**
 * ArgumentsManager delMac option argument getter method.
 *
 * @return delMac option argument
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::ArgumentsManager::getDelMac() const {
    return _delMac;
}

/**
 * ArgumentsManager isAddSet option getter.
 *
 * @return whether the add option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isAddSet() const {
    return _addSet;
}

/**
 * ArgumentsManager isUpdateSet option getter.
 *
 * @return whether the update option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isUpdateSet() const {
    return _updateSet;
}

/**
 * ArgumentsManager isRemoveSet option getter.
 *
 * @return whether the remove option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isRemoveSet() const {
    return _removeSet;
}

/**
 * ArgumentsManager isViewSet option getter.
 *
 * @return whether the view option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isViewSet() const {
    return _viewSet;
}

/**
 * ArgumentsManager isPassSet option getter.
 *
 * @return whether the password option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isPassSet() const {
    return _passSet;
}

/**
 * ArgumentsManager isDeleteSet option getter.
 *
 * @return whether the delete option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isDeleteSet() const {
    return _deleteSet;
}

/**
 * ArgumentsManager isMacSet option getter.
 *
 * @return whether the mac option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isMacSet() const {
    return _macSet;
}

/**
 * ArgumentsManager isStartSet option getter.
 *
 * @return whether the start option was set
 *
 * @author Michele Crepaldi s269551
 */
bool server::ArgumentsManager::isStartSet() const {
    return _startSet;
}
