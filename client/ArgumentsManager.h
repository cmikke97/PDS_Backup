//
// Created by Michele Crepaldi s269551 on 27/11/2020
// Finished on 27/11/2020
// Last checked on 27/11/2020
//

#ifndef CLIENT_ARGUMENTSMANAGER_H
#define CLIENT_ARGUMENTSMANAGER_H


#include <string>
#include <stdexcept>

/**
 * PDS_Backup client namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace client {
    /**
     * ArgumentsManagerError class: it describes (enumerically) all the possible Input errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class ArgumentsManagerError {
        //wrong number of parameters in input
        numberOfArguments,

        //error with an option, it is missing its option argument
        option,

        //error with an option argument inserted
        optArgument,

        //help option was set
        help
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * ArgumentsManager class
     */

    /**
     * ArgumentsManager class.
     *  Used to interpret the arguments to the main and verify their correct use.
     *
     * @author Michele Crepaldi s269551
     */
    class ArgumentsManager {
    public:
        ArgumentsManager(const ArgumentsManager &) = delete;              //copy constructor deleted
        ArgumentsManager &operator=(const ArgumentsManager &) = delete;   //copy assignment deleted
        ArgumentsManager(ArgumentsManager &&) = delete;                   //move constructor deleted
        ArgumentsManager &operator=(ArgumentsManager &&) = delete;        //move assignment deleted
        ~ArgumentsManager() = default;   //default destructor

        ArgumentsManager(int argc, char** argv);    //constructor with number of args and argument from main

        const std::string &getUsername() const;     //username getter method
        const std::string &getPassword() const;     //password getter method

        const std::string &getMac() const;          //mac getter method
        const std::string &getDestFolder() const;   //destFolder getter method

        const std::string &getServerIp() const;     //server ip address getter method
        const std::string &getSeverPort() const;    //server port getter method

        bool isUserSet() const;     //is User option set method
        bool isPassSet() const;     //is Pass option set method

        bool isRetrSet() const;     //is Retr option set method
        bool isDirSet() const;      //is Dir option set method
        bool isMacSet() const;      //is Mac option set method
        bool isAllSet() const;      //is All option set method

        bool isStartSet() const;    //is Start option set method
        bool isIpSet() const;       //is ip option set method
        bool isPortSet() const;     //is port option set method

    private:
        std::string _username;      //username optional argument
        std::string _password;      //password optional argument

        std::string _mac;           //mac address optional argument
        std::string _destFolder;    //destination folder optional argument

        std::string _serverIp;      //server ip address optional argument
        std::string _serverPort;    //server port optional argument

        bool _userSet;      //if user option was set
        bool _passSet;      //if pass option was set

        bool _retrSet;      //if retrieve option was set
        bool _dirSet;       //if the destination dir option was set
        bool _macSet;       //if mac option was set
        bool _allSet;       //if all option was set

        bool _startSet;     //if start option was set
        bool _ipSet;        //if the server ip address option was set
        bool _portSet;      //if the server port option was set

        static void _displayHelp(const std::string &programName);  //display help method
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * ArgumentsManagerException class
     */

    /**
     * ArgumentsManagerException exception class that may be returned by the ArgumentsManager class
     *  (derives from runtime_error)
     *
     * @author Michele Crepaldi s269551
     */
    class ArgumentsManagerException : public std::runtime_error {
    public:

        /**
         * arguments manager exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        ArgumentsManagerException(const std::string &msg, ArgumentsManagerError code) :
                std::runtime_error(msg), _code(code) {
        }

        /**
         * arguments manager exception destructor
         *
         * @author Michele Crepaldi s269551
         */
        ~ArgumentsManagerException() noexcept override = default;

        /**
         * function to retrieve the error code from the exception
         *
         * @return argument manager error code
         *
         * @author Michele Crepaldi s269551
         */
        ArgumentsManagerError getCode() const noexcept {
            return _code;
        }

    private:
        ArgumentsManagerError _code;  //code describing the error
    };
}

#endif //CLIENT_ARGUMENTSMANAGER_H

