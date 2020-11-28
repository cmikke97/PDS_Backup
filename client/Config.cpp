//
// Created by Michele Crepaldi s269551 on 29/09/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#include "Config.h"

#include <regex>
#include <filesystem>
#include <fstream>

#include "../myLibraries/Message.h"
#include "../myLibraries/Validator.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * default variables values definition
 */

#define MILLIS_FILESYSTEM_WATCHER 5000      //how many milliseconds to wait before re-scan the watched directory

//dimension of the event queue, a.k.a. how many events can be putted in queue at the same time
#define EVENT_QUEUE_SIZE 20

#define SECONDS_BETWEEN_RECONNECTIONS 10    //seconds to wait before client retrying connection after connection lost
#define MAX_CONNECTION_RETRIES 12           //maximum number of times the system will re-try to connect consecutively

#define TIMEOUT 15                          //seconds to wait before client-server connection timeout
#define SELECT_TIMEOUT 5                    //seconds to wait between one select and the other
#define MAX_RESPONSE_WAITING 1024           //maximum amount of messages that can be sent without response
#define TEMP_FILE_NAME_SIZE 8               //Size of the name of temporary files

#define DATABASE_PATH "../clientFiles/clientDB.sqlite"  //path of the client database
#define CA_FILE_PATH "../../TLScerts/cacert.pem"        //path of the CA to use to check the server certificate

//Maximum size (in bytes) of the file transfer chunks ('data' part of DATA messages).
//The maximum size for a protocol buffer message is 64MB, for a TCP socket it is 1GB, and for a TLS socket it is 16KB.
//So, keeping in mind that there are also other fields in the message, KEEP IT BELOW (or equal) 15KB.
#define MAX_DATA_CHUNK_SIZE 15360   //now set to 15KB


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * host dependant variables definition
 * (variables which depend on the specific host/machine which the program is running on)
 * so their default value is the empty string meaning that the user will be asked to properly specify them
 */

#define PATH_TO_WATCH ""    //path to watch for changes


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Config class methods
 */

//static variables definition
std::shared_ptr<client::Config> client::Config::config_;
std::mutex client::Config::mutex_;
std::string client::Config::path_;

/**
 * Config class path_ variable setter
 *
 * @param path path of the config file on disk
 *
 * @author Michele Crepaldi s269551
 */
void client::Config::setPath(std::string path){
    path_ = std::move(path);    //set the path_
}

/**
 * Config class singleton instance getter
 *
 * @return Config instance
 *
 * @author Michele Crepaldi s269551
 */
std::shared_ptr<client::Config> client::Config::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(config_ == nullptr) //first time, or when it was released from everybody
        config_ = std::shared_ptr<Config>(new Config());  //create the database object
    return config_;
}

/**
 * (protected) constructor ot the configuration object
 *
 * @throws ConfigException:
 *  <b>path</b> if no path was set before this call
 *
 * @author Michele Crepaldi s269551
 */
client::Config::Config() {
    if(path_.empty())   //a path must be previously set
        throw ConfigException("No path set", ConfigError::path);

    _load(); //load the configuration variables from file
}

/**
 * utility function used to add a single configuration variable line to the config file
 *
 * @param f config file fstream to add the line to
 * @param variableName name of the variable to insert
 * @param value default value of the variable to insert
 *
 * @author Michele Crepaldi s269551
 */
void addConfigVariable(std::fstream &f, const std::string &variableName, const std::string &value){
    f << variableName << " = " << value << std::endl;
}

/**
 * utility function used to add a single comment line to the config file
 *
 * @param f config file fstream to add the line to
 * @param comment comment to insert
 *
 * @author Michele Crepaldi s269551
 */
void addSingleComment(std::fstream &f, const std::string &comment){
    f << comment << std::endl;
}

/**
 * utility function to add an array of comments to the config file
 *
 * @tparam rows number of rows of the comments array (inserted by the compiler)
 * @param file config file fstream to add the lines to
 * @param comments reference to the comments array (vector) to insert into the config file
 *
 * @author Michele Crepaldi s269551
 */
template <size_t rows>
void addComments(std::fstream &file, const std::string (&comments)[rows])
{
    for (size_t i = 0; i < rows; ++i)   //for each comment in the comments array
        addSingleComment(file, comments[i]);    //add the comment to the file
    file << std::endl;
}

/**
 * utility function to add an array (matrix) of variables to the config file
 *
 * @tparam rows number of rows of the variables array (inserted by the compiler)
 * @param file config file fstream to add the lines to
 * @param variables reference to the variables array (matrix) to insert into the config file
 */
template <size_t rows>
void addVariables(std::fstream &file, const std::string (&variables)[rows][3])
{
    for (size_t i = 0; i < rows; ++i){  //for each variable in the variables array
        addSingleComment(file, variables[i][2]);    //add the comment to the file

        //add the variable (name and default value) to the file
        addConfigVariable(file, variables[i][0], variables[i][1]);

        file << std::endl;
    }
    file << std::endl;
}

/**
 * method to be used to load the configuration from file (it creates the file with default values if it does not exist)
 *
 * @throws ConfigException:
 *  <b>justCreated</b> error in case the file did not exist and a new one was just created (some variables are host
 *  dependant, for these variables there are no default values; the user has to modify the file and restart the program).
 * @throws ConfigException:
 *  <b>open</b> error in case the config file could not be opened.
 *
 * @author Michele Crepaldi s269551
 */
void client::Config::_load() {
    std::fstream file;
    std::string str, key, value;
    std::smatch m;

    //regex to format the strings got from file (get the key and value, ignoring comments and spaces)
    std::regex eKeyValue (R"(\s*(\w+)\s*=\s*([^\s]+)\s*)");

    //if the file is not there then create (and populate with defaults) it
    if(!std::filesystem::exists(path_)){
        file.open(path_, std::ios::out);    //create file

        Message::print(std::cout, "WARNING", "Configuration file does not exist",
                          "it will now be created with default values");

        //all host dependant variables
        std::string hostVariables[][3] = {  {"path_to_watch",   PATH_TO_WATCH,
                                                "# Path of the folder to back up on server"}};

        //all default-able variables
        std::string variables[][3] = {  {"database_path",                   DATABASE_PATH,
                                            "# Client Database path"},

                                        {"ca_file_path",                    CA_FILE_PATH,
                                            "# CA to use for server certificate verification"},

                                        {"millis_filesystem_watcher",       std::to_string(MILLIS_FILESYSTEM_WATCHER),
                                            "# Milliseconds the file system watcher between one folder (to watch) polling"
                                            " and the other"},

                                        {"event_queue_size",                std::to_string(EVENT_QUEUE_SIZE),
                                            "# Maximum size for the event queue (in practice how many events can be"
                                            " detected before sending them to server)"},

                                        {"seconds_between_reconnections",   std::to_string(SECONDS_BETWEEN_RECONNECTIONS),
                                            "# Seconds the client will wait between one connection attempt and the other"},

                                        {"max_connection_retries",          std::to_string(MAX_CONNECTION_RETRIES),
                                            "# Maximum number of allowed connection attempts"},

                                        {"timeout_seconds",                 std::to_string(TIMEOUT),
                                            "# Seconds to wait before the client will disconnect"},

                                        {"select_timeout_seconds",          std::to_string(SELECT_TIMEOUT),
                                            "# Seconds the client will wait between 2 subsequent selects on the socket"},

                                        {"max_response_waiting",            std::to_string(MAX_RESPONSE_WAITING),
                                            "# Maximum number of messages waiting for a server response allowed"},

                                        {"tmp_file_name_size",              std::to_string(TEMP_FILE_NAME_SIZE),
                                            "# Temporary files name size"},

                                        {"max_data_chunk_size",             std::to_string(MAX_DATA_CHUNK_SIZE),
                                            "# Maximum size (in bytes) of the file transfer chunks ('data' part of DATA"
                                            " messages)\n"
                                            "# the maximum size for a protocol buffer message is 64MB, for a TCP socket"
                                            " it is 1GB,\n"
                                            "# and for a TLS socket it is 16KB.\n"
                                            "# So, keeping in mind that there are also other fields in the message,\n"
                                            "# KEEP IT BELOW (or equal) 15KB."}};


        //comments on top of the file
        std::string initial_comments[] = {          "###########################################################################",
                                                    "#                                                                         #",
                                                    "#        -Configuration file for the CLIENT of PDS_Backup project-        #",
                                                    "#                   (rows preceded by '#' are comments)                   #",
                                                    "#                                                                         #",
                                                    "###########################################################################"};

        //comments before the host dependant variables
        std::string host_variables_comments[] = {   "###########################################################################",
                                                    "#        -Host specific variables: no default values are provided-        #",
                                                    "###########################################################################"};

        //comments before the default-able variables
        std::string variables_comments[] = {        "###########################################################################",
                                                    "#                             Other variables                             #",
                                                    "#        -  in case of empty fields default values will be used  -        #",
                                                    "###########################################################################"};

        //comments at the end of the file
        std::string final_comments[]  = {           "###########################################################################",
                                                    "#                                                                         #",
                                                    "#        -              Configuration file finished              -        #",
                                                    "#                                                                         #",
                                                    "###########################################################################"};

        addComments(file, initial_comments);    //add initial comments
        addComments(file, host_variables_comments); //add host variables comments
        addVariables(file, hostVariables);  //add host variables
        addComments(file, variables_comments);  //add variables comments
        addVariables(file, variables);  //add other variables
        addComments(file, final_comments);  //add final comments

        file.close();   //close file

        //when I create the file some values cannot be defaulted, since they are host dependant;
        //ask the user to modify them shutting down the program
        throw ConfigException("Configuration file created, modify it and restart.", ConfigError::justCreated);
    }

    //open configuration file as input
    file.open(path_, std::ios::in);

    if(file.is_open()) {
        while(getline(file,str)) {  //for each line
            if(std::regex_match (str,m,eKeyValue)){   //regex match
                //get the key and the value
                key = m[1];
                value = m[2];

                std::transform(key.begin(),key.end(),key.begin(), ::tolower);   //convert all characters in upper case

                /*
                 * +---------------------------------------------------------------------------------------------------+
                 * folder variables
                 */
                if(key == "path_to_watch"){
                    if(Validator::validatePath(value))
                        _path_to_watch = value;
                }

                /*
                 * +---------------------------------------------------------------------------------------------------+
                 * file variables
                 */
                else if(key == "database_path") {
                    if(Validator::validatePath(value))
                        _database_path = value;
                }
                else if(key == "ca_file_path") {
                    if(Validator::validatePath(value))
                        _ca_file_path = value;
                }

                /*
                 * +---------------------------------------------------------------------------------------------------+
                 * other variables (all positive integers)
                 */
                else {
                    //check if the value is a positive unsigned integer
                    if(!Validator::validateUint(value))
                        //if it is not just skip it
                        continue;

                    //assign it to the corresponding unsigned integer member variable
                    if (key == "millis_filesystem_watcher")
                        _millis_filesystem_watcher = static_cast<unsigned int>(stoul(value));
                    else if (key == "event_queue_size")
                        _event_queue_size = static_cast<unsigned int>(stoul(value));
                    else if (key == "seconds_between_reconnections")
                        _seconds_between_reconnections = static_cast<unsigned int>(stoul(value));
                    else if (key == "max_connection_retries")
                        _max_connection_retries = static_cast<unsigned int>(stoul(value));
                    else if (key == "timeout_seconds")
                        _timeout_seconds = static_cast<unsigned int>(stoul(value));
                    else if (key == "select_timeout_seconds")
                        _select_timeout_seconds = static_cast<unsigned int>(stoul(value));
                    else if (key == "max_response_waiting")
                        _max_response_waiting = static_cast<unsigned int>(stoul(value));
                    else if (key == "tmp_file_name_size")
                        _tmp_file_name_size = static_cast<unsigned int>(stoul(value));
                    else if (key == "max_data_chunk_size")
                        _max_data_chunk_size = static_cast<unsigned int>(stoul(value));
                }
            }
        }
        file.close();   //close file
    }
    else{
        //if the file could not be opened throw exception
        throw ConfigException("Could not open configuration file", ConfigError::open);
    }
}

/**
 * path to watch (folder) getter (HOST specific, this has no default values; so if no value was provided an exception
 *  will be thrown)
 *
 * @return path to watch
 *
 * @throws ConfigException:
 *  <b>pathToWatch</b> error in case no value was provided (there are no defaults for this variable) OR if the value
 *  provided references a non existing directory or something which is not a directory.
 *
 * @author Michele Crepaldi s269551
 */
const std::string& client::Config::getPathToWatch() {
    //value required.. if it is empty throw exception (there is no default value for this variable)
    if(_path_to_watch.empty())
        throw ConfigException("Path to watch was not set", ConfigError::pathToWatch);

    //if the path to watch set references a non-existent folder
    if(!std::filesystem::exists(_path_to_watch))
        throw ConfigException("Path to watch does not exist", ConfigError::pathToWatch);

    //if the path to watch set references something which is not a folder
    if(!std::filesystem::is_directory(_path_to_watch))
        throw ConfigException("Path to watch is not a directory", ConfigError::pathToWatch);

    return _path_to_watch;
}

/**
 * database path getter (if no value was provided in the config file use a default one)
 *
 * @return database path
 *
 * @author Michele Crepaldi s269551
 */
const std::string& client::Config::getDatabasePath() {
    if(_database_path.empty())
        _database_path = DATABASE_PATH;   //set to default

    //database path will be verified by the Database class

    return _database_path;
}

/**
 * CA file path getter (if no value was provided in the config file use a default one)
 *
 * @return CA file path
 *
 * @author Michele Crepaldi s269551
 */
const std::string& client::Config::getCAFilePath(){
    if(_ca_file_path.empty())
        _ca_file_path = CA_FILE_PATH;   //set to default

    //the CA file path will be verified by the Socket class

    return _ca_file_path;
}

/**
 * millis filesystem watcher getter (if no value was provided in the config file use a default one)
 *
 * @return millis filesystem watcher
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getMillisFilesystemWatcher() {
    if(_millis_filesystem_watcher == 0)
        _millis_filesystem_watcher = MILLIS_FILESYSTEM_WATCHER;   //set to default

    return _millis_filesystem_watcher;
}

/**
 * event queue size getter (if no value was provided in the config file use a default one)
 *
 * @return event queue size
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getEventQueueSize() {
    if(_event_queue_size == 0)
        _event_queue_size = EVENT_QUEUE_SIZE;   //set to default

    return _event_queue_size;
}

/**
 * seconds between reconnections getter (if no value was provided in the config file use a default one)
 *
 * @return seconds between reconnections
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getSecondsBetweenReconnections() {
    if(_seconds_between_reconnections == 0)
        _seconds_between_reconnections = SECONDS_BETWEEN_RECONNECTIONS;   //set to default

    return _seconds_between_reconnections;
}

/**
 * max connection retries getter (if no value was provided in the config file use a default one)
 *
 * @return max connection retries
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getMaxConnectionRetries() {
    if(_max_connection_retries == 0)
        _max_connection_retries = MAX_CONNECTION_RETRIES;   //set to default

    return _max_connection_retries;
}

/**
 * timeout seconds getter (if no value was provided in the config file use a default one)
 *
 * @return timeout seconds
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getTimeoutSeconds() {
    if(_timeout_seconds == 0)
        _timeout_seconds = TIMEOUT;   //set to default

    return _timeout_seconds;
}

/**
 * select timeout seconds getter (if no value was provided in the config file use a default one)
 *
 * @return select timeout seconds
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getSelectTimeoutSeconds() {
    if(_select_timeout_seconds == 0)
        _select_timeout_seconds = SELECT_TIMEOUT;   //set to default

    return _select_timeout_seconds;
}

/**
 * max response waiting getter (if no value was provided in the config file use a default one)
 *
 * @return max response waiting
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getMaxResponseWaiting() {
    if(_max_response_waiting == 0)
        _max_response_waiting = MAX_RESPONSE_WAITING;   //set to default

    return _max_response_waiting;
}

/**
 * temp file name size getter (if no value was provided in the config file use a default one)
 *
 * @return temp file name size
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getTmpFileNameSize() {
    if(_tmp_file_name_size == 0)
        _tmp_file_name_size = TEMP_FILE_NAME_SIZE;   //set to default

    return _tmp_file_name_size;
}

/**
 * max data chunk size getter (if no value was provided in the config file use a default one)
 *
 * @return max data chunk size
 *
 * @author Michele Crepaldi s269551
 */
unsigned int client::Config::getMaxDataChunkSize() {
    if(_max_data_chunk_size == 0)
        _max_data_chunk_size = MAX_DATA_CHUNK_SIZE;   //set to default

    return _max_data_chunk_size;
}