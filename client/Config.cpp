//
// Created by michele on 29/09/2020.
//

#include <regex>
#include <filesystem>
#include <iostream>
#include <utility>
#include "Config.h"
#include "../myLibraries/TS_Message.h"

//default values
#define MILLIS_FILESYSTEM_WATCHER 5000 //how many milliseconds to wait before re-scan the watched directory
#define EVENT_QUEUE_SIZE 20 //dimension of the event queue, a.k.a. how many events can be putted in queue at the same time
#define SECONDS_BETWEEN_RECONNECTIONS 10 //seconds to wait before client retrying connection after connection lost
#define MAX_CONNECTION_RETRIES 12 //maximum number of times the system will re-try to connect consecutively
#define MAX_SERVER_ERROR_RETRIES 5 //maximum number of times the system will try re-sending a message (and all messages sent after it to maintain the final outcome) after an internal server error
#define MAX_AUTH_ERROR_RETRIES 5 //maximum number of times the system will re-try the authentication after an internal server error
#define TIMEOUT 30 //seconds to wait before client-server connection timeout
#define SELECT_TIMEOUT 5 //seconds to wait between one select and the other
#define MAX_RESPONSE_WAITING 1024 //maximum amount of messages that can be sent without response
#define DATABASE_PATH "../clientFiles/clientDB.sqlite"  //path of the client database
#define CA_FILE_PATH "../../TLScerts/cacert.pem"    //path of the CA to use to check the server certificate
#define PATH_TO_WATCH "C:/Users/michele/Desktop/programmazione_di_sistema_backup"    //path to watch for changes

//20KB
#define MAX_DATA_CHUNK_SIZE 20480   //Maximum size of the data part of DATA messages (it corresponds to the size of the buffer used to read from the file)
//the maximum size for a protocol buffer message is 64MB (for a TCP socket it is 1GB), so keep it less than that (keeping in mind that there are also other fields in the message)

//static variables definition
std::shared_ptr<client::Config> client::Config::config_;
std::mutex client::Config::mutex_;

/**
 * Config class singleton instance getter
 *
 * @param path path of the config file on disk
 * @return Config instance
 *
 * @author Michele Crepaldi s269551
 */
std::shared_ptr<client::Config> client::Config::getInstance(const std::string &path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(config_ == nullptr)
        config_ = std::shared_ptr<Config>(new Config(path));  //create the database object
    return config_;
}

/**
 * (protected) constructor ot the configuration object
 *
 * @param path path of the file to retrieve configuration from
 *
 * @author Michele Crepaldi s269551
 */
client::Config::Config(std::string path) : path_(std::move(path)) {
    load(path_);
}

void addConfigVariable(std::fstream &f, const std::string &variableName, const std::string &value){
    f << variableName << " = " << value << std::endl;
}

void addComment(std::fstream &f, const std::string &comment){
    f << comment << std::endl;
}

/**
 * function to load the configuration from the file (it creates it with default values if it does not exist)
 *
 * @param configFilePath path of the file to load configuration from
 *
 * @author Michele Crepaldi s269551
 */
void client::Config::load(const std::string &configFilePath) {
    std::fstream file;
    std::string str, key, value;
    std::smatch m;
    std::regex e (R"(\s*(\w+)\s*=\s*(.+[/\w])\s*)");

    //if the file is not there then create (and populate with defaults) it
    if(!std::filesystem::directory_entry(configFilePath).exists()){
        file.open(configFilePath, std::ios::out);

        TS_Message::print(std::cout, "WARNING", "Configuration file does not exist", "it will now be created with default values");

        std::string variables[13][3] = {{"path_to_watch",                   PATH_TO_WATCH,                                  "# Path to back up on server"},
                                        {"database_path",                   DATABASE_PATH,                                  "# Client Databse path"},
                                        {"ca_file_path",                    CA_FILE_PATH,                                   "# CA to use for server certificate verification"},
                                        {"millis_filesystem_watcher",       std::to_string(MILLIS_FILESYSTEM_WATCHER),      "# Milliseconds the file system watcher will wait every cycle"},
                                        {"event_queue_size",                std::to_string(EVENT_QUEUE_SIZE),               "# Max events queue size"},
                                        {"seconds_between_reconnections",   std::to_string(SECONDS_BETWEEN_RECONNECTIONS),  "# Seconds the client will wait between successive connection attempts"},
                                        {"max_connection_retries",          std::to_string(MAX_CONNECTION_RETRIES),         "# Maximum number of allowed connection attempts"},
                                        {"max_server_error_retries",        std::to_string(MAX_SERVER_ERROR_RETRIES),       "# Maximum number of message retransmissions of the same message after a server error"},
                                        {"max_auth_error_retries",          std::to_string(MAX_AUTH_ERROR_RETRIES),         "# Unused, to remove.."},
                                        {"timeout_seconds",                 std::to_string(TIMEOUT),                        "# Seconds to wait before the client will disconnect"},
                                        {"select_timeout_seconds",          std::to_string(SELECT_TIMEOUT),                 "# Seconds the client will wait between 2 selects on the socket"},
                                        {"max_response_waiting",            std::to_string(MAX_RESPONSE_WAITING),           "# Maximum number of messages waiting for a server response allowed"},
                                        {"max_data_chunk_size",             std::to_string(MAX_DATA_CHUNK_SIZE),            "# Maximum size (in bytes) of the file transfer chunks ('data' part of DATA messages)"
                                                                                                                            "\n# the maximum size for a protocol buffer message is 64MB (for a TCP socket it is 1GB)"
                                                                                                                            "\n# so keep it less than that (keeping in mind that there are also other fields in the message)"}};


        std::string initial_comments[] = {  "###########################################################################",
                                            "#                                                                         #",
                                            "#         Configuration file for the client of PDS_Backup project         #",
                                            "#        -  in case of empty fields default values will be used  -        #",
                                            "#                   (rows preceded by '#' are comments)                   #",
                                            "#                                                                         #",
                                            "###########################################################################"};

        std::string final_comments[]  = {   "###########################################################################",
                                            "#                                                                         #",
                                            "#        -              Configuration file finished              -        #",
                                            "#                                                                         #",
                                            "###########################################################################"};

        //add initial comments
        for(const auto & initial_comment : initial_comments)
            addComment(file, initial_comment);
        file << std::endl;

        for(auto & variable : variables){
            addComment(file, variable[2]);
            addConfigVariable(file, variable[0], variable[1]);
            file << std::endl;
        }
        file << std::endl;

        //add final comments
        for(const auto & final_comment : final_comments)
            addComment(file, final_comment);
        file << std::endl;

        file.close();
    }

    //open configuration file
    file.open(configFilePath, std::ios::in);

    if(file.is_open()) {
        //for each line
        while(getline(file,str)) {
            //regex match
            if(std::regex_match (str,m,e)){
                //get the key and the value
                key = m[1];
                value = m[2];

                //convert all characters in upper case
                std::transform(key.begin(),key.end(),key.begin(), ::tolower);

                //folders
                if(key == "path_to_watch"){
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    value = std::regex_replace(value, std::regex("\\\\"), "/");

                    //delete the tailing / that may be there at the end of the path
                    std::smatch mtmp;
                    std::regex etmp ("(.*)\\/$");
                    if(std::regex_match (value,m,e))    //if i fine the trailing "/" i remove it
                        path_to_watch = m[1];
                    else
                        path_to_watch = value;
                }

                //files
                if(key == "database_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    database_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }
                if(key == "ca_file_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    ca_file_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }

                //integers
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
                if(key == "max_data_chunk_size")
                    max_data_chunk_size = stoi(value);

            }
        }
        file.close();
    }
    else{
        //if the file could not be opened
        throw ConfigException("Could not open configuration file", configError::open);
    }
}

/**
 * path to watch getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
std::string& client::Config::getPathToWatch() {
    if(path_to_watch.empty())
        path_to_watch = PATH_TO_WATCH;

    //if the path to watch set references a non-existant folder
    if(!std::filesystem::exists(path_to_watch))
        throw ConfigException("Path to watch does not exists", configError::pathToWatch);

    return path_to_watch;
}

/**
 * database path getter (if no value was provided in the config file use a default one)
 *
 * @return database path
 *
 * @author Michele Crepaldi s269551
 */
std::string& client::Config::getDatabasePath() {
    if(database_path.empty())
        database_path = DATABASE_PATH;

    //the database path will be verified by the Database class

    return database_path;
}

/**
 * CA file path getter (if no value was provided in the config file use a default one)
 *
 * @return CA file path
 *
 * @author Michele Crepaldi s269551
 */
std::string& client::Config::getCAFilePath(){
    if(ca_file_path.empty())
        ca_file_path = CA_FILE_PATH;

    //the CA file path will be verified by the Socket class

    return ca_file_path;
}

/**
 * millis filesystem watcher getter (if no value was provided in the config file use a default one)
 *
 * @return millis filesystem watcher
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getMillisFilesystemWatcher() {
    if(millis_filesystem_watcher == 0)
        millis_filesystem_watcher = MILLIS_FILESYSTEM_WATCHER;

    return millis_filesystem_watcher;
}

/**
 * event queue size getter (if no value was provided in the config file use a default one)
 *
 * @return event queue size
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getEventQueueSize() {
    if(event_queue_size == 0)
        event_queue_size = EVENT_QUEUE_SIZE;

    return event_queue_size;
}

/**
 * seconds between reconnections getter (if no value was provided in the config file use a default one)
 *
 * @return seconds between reconnections
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getSecondsBetweenReconnections() {
    if(seconds_between_reconnections == 0)
        seconds_between_reconnections = SECONDS_BETWEEN_RECONNECTIONS;

    return seconds_between_reconnections;
}

/**
 * max connection retries getter (if no value was provided in the config file use a default one)
 *
 * @return max connection retries
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getMaxConnectionRetries() {
    if(max_connection_retries == 0)
        max_connection_retries = MAX_CONNECTION_RETRIES;

    return max_connection_retries;
}

/**
 * max server error retries getter (if no value was provided in the config file use a default one)
 *
 * @return max server error retries
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getMaxServerErrorRetries() {
    if(max_server_error_retries == 0)
        max_server_error_retries = MAX_SERVER_ERROR_RETRIES;

    return max_server_error_retries;
}

/**
 * max auth error retries getter (if no value was provided in the config file use a default one)
 *
 * @return max auth error retries
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getMaxAuthErrorRetries() {
    if(max_auth_error_retries == 0)
        max_auth_error_retries = MAX_AUTH_ERROR_RETRIES;

    return max_auth_error_retries;
}

/**
 * timeout seconds getter (if no value was provided in the config file use a default one)
 *
 * @return timeout seconds
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getTimeoutSeconds() {
    if(timeout_seconds == 0)
        timeout_seconds = TIMEOUT;

    return timeout_seconds;
}

/**
 * select timeout seconds getter (if no value was provided in the config file use a default one)
 *
 * @return select timeout seconds
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getSelectTimeoutSeconds() {
    if(select_timeout_seconds == 0)
        select_timeout_seconds = SELECT_TIMEOUT;

    return select_timeout_seconds;
}

/**
 * max response waiting getter (if no value was provided in the config file use a default one)
 *
 * @return max response waiting
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getMaxResponseWaiting() {
    if(max_response_waiting == 0)
        max_response_waiting = MAX_RESPONSE_WAITING;

    return max_response_waiting;
}

/**
 * max data chunk size getter (if no value was provided in the config file use a default one)
 *
 * @return max data chunk size
 *
 * @author Michele Crepaldi s269551
 */
int client::Config::getMaxDataChunkSize() {
    if(max_data_chunk_size == 0)
        max_data_chunk_size = MAX_DATA_CHUNK_SIZE;

    return max_data_chunk_size;
}
