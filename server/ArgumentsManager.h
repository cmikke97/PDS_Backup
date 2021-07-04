//
// Created by Michele Crepaldi s269551 on 27/11/2020
// Finished on 27/11/2020
// Last checked on 27/11/2020
//

#ifndef SERVER_ARGUMENTSMANAGER_H
#define SERVER_ARGUMENTSMANAGER_H


#include <string>
#include <stdexcept>

/**
 * PDS_Backup server namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace server {
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
        ArgumentsManager(const ArgumentsManager &) = delete;            //copy constructor deleted
        ArgumentsManager &operator=(const ArgumentsManager &) = delete; //copy assignment deleted
        ArgumentsManager(ArgumentsManager &&) = delete;                 //move constructor deleted
        ArgumentsManager &operator=(ArgumentsManager &&) = delete;      //move assignment deleted
        ~ArgumentsManager() = default;   //default destructor

        ArgumentsManager(int argc, char** argv);    //constructor with number of args and argument from main

        const std::string &getUsername() const;     //username getter method
        const std::string &getPassword() const;     //password getter method
        const std::string &getDelUsername() const;  //delUser (user to delete) getter method
        const std::string &getDelMac() const;       //delMac (mac to delete) getter method

        bool isAddSet() const;      //is Add option set method
        bool isUpdateSet() const;   //is Update option set method
        bool isRemoveSet() const;   //is Remove option set method
        bool isViewSet() const;     //is View option set method
        bool isPassSet() const;     //is Pass option set method
        bool isDeleteSet() const;   //is Delete option set method
        bool isMacSet() const;      //is Mac option set method
        bool isStartSet() const;    //is Start option set method

    private:
        std::string _username;      //username optional argument
        std::string _password;      //password optional argument
        std::string _delUsername;   //delUsername optional argument
        std::string _delMac;        //delMac optional argument

        bool _addSet;       //if add option was set
        bool _updateSet;    //if update option was set
        bool _passSet;      //if pass option was set

        bool _removeSet;    //if remove option was set
        bool _viewSet;      //if view option was set

        bool _deleteSet;    //if delete option was set
        bool _macSet;       //if mac option was set

        bool _startSet;     //if start option was set

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

#endif //SERVER_ARGUMENTSMANAGER_H
