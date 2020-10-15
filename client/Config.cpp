//
// Created by michele on 29/09/2020.
//

#include <regex>
#include <filesystem>
#include <iostream>
#include "Config.h"

//default values
#define MILLIS_FILESYSTEM_WATCHER 5000 //how many milliseconds to wait before re-scan the watched directory
#define EVENT_QUEUE_SIZE 20 //dimension of the event queue, a.k.a. how many events can be putted in queue at the same time
#define SECONDS_BETWEEN_RECONNECTIONS 10 //seconds to wait before client retrying connection after connection lost
#define MAX_CONNECTION_RETRIES 12 //maximum number of times the system will re-try to connect consecutively
#define MAX_SERVER_ERROR_RETRIES 5 //maximum number of times the system will try re-sending a message (and all messages sent after it to maintain the final outcome) after an internal server error
#define MAX_AUTH_ERROR_RETRIES 5 //maximum number of times the system will re-try the authentication after an internal server error
#define TIMEOUT 300 //seconds to wait before client-server connection timeout (5 minutes)
#define SELECT_TIMEOUT 5 //seconds to wait between one select and the other
#define MAX_RESPONSE_WAITING 1024 //maximum amount of messages that can be sent without response
#define DATABASE_PATH "./clientDB/clientDB.sqlite"
#define PATH_TO_WATCH ""

void Config::load(const std::string &configFilePath) {
    std::fstream file;
    std::string str, key, value;
    std::smatch m;
    std::regex e ("\\s*(\\w+)\\s*=\\s*(.+[/\\w])\\s*");

    //if the file is not there then create (and populate with defaults) it
    if(!std::filesystem::directory_entry(configFilePath).exists()){
        file.open(configFilePath, std::ios::out);

        std::cout << "Configuration file not found." << std::endl;

        file << "#Configuration file for the client of PDS_Backup project" << std::endl;
        file << "#- in case of empty fields default values will be used -" << std::endl;
        file << std::endl;
        file << "path_to_watch = " << PATH_TO_WATCH << std::endl;
        file << "database_path = " << DATABASE_PATH << std::endl;
        file << std::endl;
        file << "millis_filesystem_watcher = " << MILLIS_FILESYSTEM_WATCHER << std::endl;
        file << "event_queue_size = " << EVENT_QUEUE_SIZE << std::endl;
        file << std::endl;
        file << "seconds_between_reconnections = " << SECONDS_BETWEEN_RECONNECTIONS << std::endl;
        file << "max_connection_retries = " << MAX_CONNECTION_RETRIES << std::endl;
        file << "max_server_error_retries = " << MAX_SERVER_ERROR_RETRIES << std::endl;
        file << "max_auth_error_retries = " << MAX_AUTH_ERROR_RETRIES << std::endl;
        file << "timeout_seconds = " << TIMEOUT << std::endl;
        file << "select_timeout_seconds = " << SELECT_TIMEOUT << std::endl;
        file << "max_response_waiting = " << MAX_RESPONSE_WAITING << std::endl;
        file << std::endl;
        file << "#Configuration file finished" << std::endl;

        std::cout << "Configuration file created!" << std::endl;
        std::cout << "Program will now continue with default settings." << std::endl;
        std::cout << "Consider closing program and modifying config file." << std::endl;
        std::cout << "You can find it here: " << configFilePath << std::endl;

        file.close();
        return;
    }

    //open configuration file
    file.open(configFilePath, std::ios::in);

    if(file.is_open()) {
        //for each line
        while(getline(file,str)) {
            if(std::regex_match (str,m,e)){
                key = m[1];
                value = m[2];

                //convert all characters in upper case
                std::transform(key.begin(),key.end(),key.begin(), ::tolower);

                if(key == "path_to_watch"){
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    value = std::regex_replace(value, std::regex("\\\\"), "/");

                    //delete the tailing / that may be there at the end of the path
                    std::smatch mtmp;
                    std::regex etmp ("(.*)\\/$");
                    if(std::regex_match (value,m,e))
                        path_to_watch = m[1];
                    else
                        path_to_watch = value;
                }
                if(key == "database_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    database_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }
                if(key == "millis_filesystem_watcher")
                    millis_filesystem_watcher = stoi(value);
                if(key == "event_queue_size")
                    event_queue_size = stoi(value);
                if(key == "seconds_between_reconnections")
                    seconds_between_reconnections = stoi(value);
                if(key == "max_connection_retries")
                    max_connection_retries = stoi(value);
                if(key == "max_server_error_retries")
                    max_server_error_retries = stoi(value);
                if(key == "max_auth_error_retries")
                    max_auth_error_retries = stoi(value);
                if(key == "timeout_seconds")
                    timeout_seconds = stoi(value);
                if(key == "select_timeout_seconds")
                    select_timeout_seconds = stoi(value);
                if(key == "max_response_waiting")
                    max_response_waiting = stoi(value);
            }
        }
        file.close();
    }
    else{
        std::cerr << "Could not open configuration file." << std::endl;
        std::cout << "Program will now continue with default settings." << std::endl;
    }
}

const std::string Config::getPathToWatch() {
    if(path_to_watch != "")
        return path_to_watch;
    else
        return std::move(std::string(PATH_TO_WATCH));
}

const std::string Config::getDatabasePath() {
    if(database_path != "")
        return database_path;
    else
        return std::move(std::string(DATABASE_PATH));
}

int Config::getMillisFilesystemWatcher() {
    if(millis_filesystem_watcher != 0)
        return millis_filesystem_watcher;
    else
        return MILLIS_FILESYSTEM_WATCHER;
}

int Config::getEventQueueSize() {
    if(event_queue_size != 0)
        return event_queue_size;
    else
        return EVENT_QUEUE_SIZE;
}

int Config::getSecondsBetweenReconnections() {
    if(seconds_between_reconnections != 0)
        return seconds_between_reconnections;
    else
        return SECONDS_BETWEEN_RECONNECTIONS;
}

int Config::getMaxConnectionRetries() {
    if(max_connection_retries != 0)
        return max_connection_retries;
    else
        return MAX_CONNECTION_RETRIES;
}

int Config::getMaxServerErrorRetries() {
    if(max_server_error_retries != 0)
        return max_server_error_retries;
    else
        return MAX_SERVER_ERROR_RETRIES;
}

int Config::getMaxAuthErrorRetries() {
    if(max_auth_error_retries != 0)
        return max_auth_error_retries;
    else
        return MAX_AUTH_ERROR_RETRIES;
}

int Config::getTimeoutSeconds() {
    if(timeout_seconds != 0)
        return timeout_seconds;
    else
        return TIMEOUT;
}

int Config::getSelectTimeoutSeconds() {
    if(select_timeout_seconds != 0)
        return select_timeout_seconds;
    else
        return SELECT_TIMEOUT;
}

int Config::getMaxResponseWaiting() {
    if(max_response_waiting != 0)
        return max_response_waiting;
    else
        return MAX_RESPONSE_WAITING;
}
