//
// Created by Michele Crepaldi s269551 on 14/11/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <string>


/**
 * Validator class. Used to validate user input (command arguments)
 *
 * @author Michele Crepaldi s269551
 */
class Validator {
public:
    //validator static methods

    static bool validateOptArg(const char *optArg);
    static bool validateUsername(std::string &username);
    static bool validatePassword(std::string &password);
    static bool validateMacAddress(std::string &mac);
    static bool validateIPAddress(std::string &ipAddress);
    static bool validatePath(std::string &folder);
    static bool validateUint(std::string &intgerString);
    static bool validatePort(std::string &port);
    static bool validateLastWriteTime(std::string &lastWriteTime);
};


#endif //VALIDATOR_H
