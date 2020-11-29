//
// Created by Michele Crepaldi s269551 on 14/11/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#include "Validator.h"

#include <regex>
#include <iostream>

#include "Message.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Validator class methods
 */

/**
 * function used to check that the inserted optional cmd argument is actually valid (syntactically)
 *  -> it has no heading '-' (so it is not a command)
 *
 * @param optArg optional argument to be checked
 * @return whether the specified argument is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validateOptArg(const char *optArg){
    if(optArg[0] == '-')    //check if it actually is a valid argument (only syntactically)
        return false;

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) username
 *
 * @param username username to be checked
 * @return whether the specified username is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validateUsername(std::string &username){
    std::smatch m;  //regex match
    std::regex e(R"(^[a-zA-Z0-9_@+-.]+$)"); //regular expression to represent a valid username

    if (!std::regex_match(username, m, e))  //check if it actually is a valid username (only syntactically)
        return false;

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) password
 *
 * @param password password to be checked
 * @return whether the specified password is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validatePassword(std::string &password){
    std::smatch m;  //regex match
    std::regex e(R"(^[a-zA-Z0-9~`!@#$%^&*()_\-+={[}\]|\\:;"'<,>.?/]+$)");   //regular expression to represent a valid password

    if (!std::regex_match(password, m, e))  //check if it actually is a valid password (only syntactically)
        return false;

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) mac address and to transform
 *  it to a consistent mac address representation
 *
 *  <p><b>examples of transformations applied:</b>
 *  <br>:::::               ->  0:0:0:0:0:0
 *  <br>00:00:00:00:00:00   ->  0:0:0:0:0:0
 *  <br>01:02:00:20:10:00   ->  1:2:0:20:10:0
 *  <br>a:b::c::            ->  a:b:0:c:0:0
 *  </p>
 *
 * @param mac mac to be checked/translated
 * @return whether the specified mac is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validateMacAddress(std::string &mac){
    std::smatch m;  //regex match
    std::regex e(R"(^(([0-9a-fA-F]){0,2}:){5}([0-9a-fA-F]){0,2}$)");    //regular expression to represent a valid mac address

    std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower); //convert all characters to lower case

    if (!std::regex_match(mac, m, e))   //check if it actually is a valid mac address (only syntactically)
        return false;

    //transform the original mac address to a specific representation

    mac = std::regex_replace(mac, std::regex(R"(0(?=[0-9a-z]))"), "");  //remove all the heading zeros
    mac = std::regex_replace(mac, std::regex(R"(:(?=:)|:$)"), ":0");    //add all the needed zeros
    mac = std::regex_replace(mac, std::regex(R"(^:)"), "0:");           //add all the needed zeros

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) IP address
 *
 * @param ipAddress IP address to be checked
 * @return whether the specified IP address is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validateIPAddress(std::string &ipAddress){
    std::smatch m;  //regex match
    std::regex e(R"(^(localhost|(\d{1,3}\.){3}(\d{1,3}))$)");   //regular expression to represent a valid IP address

    if (!std::regex_match(ipAddress, m, e)) //check if it actually is a valid IP address (only syntactically)
        return false;

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) path and to
 *  transform it to a consistent (for the whole program) path representation ('\' -> '/'; no trailing "/")
 *
 * @param path path to be checked/translated
 * @return whether the specified path is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validatePath(std::string &path){
    std::smatch m;  //regex match
    std::regex e(R"(^(?:(?:\w:)|(?:\/))[^<>"|?*:]*$)"); //regular expression to represent a valid folder name

    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
    //(windows vs. unix)
    path = std::regex_replace(path, std::regex(R"(\\)"), "/");

    //delete the tailing "/" that may be there at the end of the path
    path = std::regex_replace(path, std::regex(R"(\/$)"), "");

    if (!std::regex_match(path, m, e))    //check if it actually is a valid folder (only syntactically)
        return false;

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) unsigned integer
 *
 * @param unsignedIntger unsigned integer to be checked
 * @return whether the specified unsigned integer is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validateUint(std::string &unsignedIntger){
    std::smatch m;  //regex match
    std::regex e(R"(^(?:\d){1,10}$)"); //regular expression to represent a valid unsigned integer

    //check if it actually is a valid unsigned integer (only syntactically)
    if (!std::regex_match(unsignedIntger, m, e))
        return false;

    //check if it actually is a valid unsigned integer (check if it is in the 0-UINT32_MAX range)

    //convert the string representation of the integer to an unsigned long
    //(there is no stoui for unsigned integers)

    unsigned long l = stoul(unsignedIntger);
    if (l<0 || l > UINT32_MAX)  //check if the value provided fits in an unsigned integer
        return false;

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) port
 *
 * @param port port to be checked
 * @return whether the specified port is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validatePort(std::string &port){
    std::smatch m;  //regex match
    std::regex e(R"(^(?:\d){1,5}$)"); //regular expression to represent a valid port name

    if (!std::regex_match(port, m, e))  //check if it actually is a valid port (only syntactically)
        return false;

    if(stoi(port) < 1 || stoi(port) > 65535)//check if it actually is a valid port (check if it is in the 1-65535 range)
        return false;

    return true;
}

/**
 * function used to check that the provided argument is actually a valid (syntactically) time
 *
 * @param port port to be checked
 * @return whether the specified port is syntactically correct or not
 *
 * @author Michele Crepaldi s269551
 */
bool Validator::validateLastWriteTime(std::string &lastWriteTime){
    std::smatch m;  //regex match

    //regular expression to represent a valid time name
    std::regex e(R"(^(?:\w+), \d{2} (?:\w+) \d{4} \d{2}:\d{2} \w*$)");

    if (!std::regex_match(lastWriteTime, m, e)) //check if it actually is a valid time (only syntactically)
        return false;

    return true;
}