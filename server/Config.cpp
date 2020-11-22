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


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * default-able variables value definition
 */

#define LISTEN_QUEUE 8              //Size of the accept listen queue
#define N_THREADS 4                 //Number of single server threads (apart from the accepting thread)
#define SOCKET_QUEUE_SIZE 10        //Maximum socket queue size
#define SELECT_TIMEOUT_SECONDS 5    //Seconds the client will wait between 2 selects on the socket
#define TIMEOUT_SECONDS 30          //Seconds the server will wait before disconnecting client
#define TEMP_FILE_NAME_SIZE 8       //Size of the name of temporary files

#define PASSWORD_DATABASE_PATH "../serverFiles/passwordDB.sqlite"       //Password database path
#define DATABASE_PATH "../serverFiles/serverDB.sqlite"                  //Server Database path
#define CERTIFICATE_PATH "../../TLScerts/server_cert.pem"               //Server certificate path
#define PRIVATEKEY_PATH "../../TLScerts/server_pkey.pem"                //Server private key path
#define CA_FILE_PATH "../../TLScerts/cacert.pem"                        //CA to use for server certificate verification

//Maximum size of the data part of DATA messages (it corresponds to the size of the buffer used to read from the file)
//the maximum size for a protocol buffer message is 64MB (for a TCP socket it is 1GB), so keep it less than that
//(keeping in mind that there are also other fields in the message)
#define MAX_DATA_CHUNK_SIZE 20480   //now set to 20KB


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * host dependant variables definition
 * (variables which depend on the specific host/machine which the program is running on)
 * so their default value is the empty string meaning that the user will be asked to properly specify them
 */

#define SERVER_PATH ""  //Server base path
#define TEMP_PATH ""    //Temporary path where to put temporary files


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Config class methods
 */

//static variables definition

std::shared_ptr<server::Config> server::Config::config_;
std::mutex server::Config::mutex_;
std::string server::Config::path_;

/**
 * Config class path_ variable setter
 *
 * @param path path of the config file on disk
 *
 * @author Michele Crepaldi s269551
 */
void server::Config::setPath(std::string path){
    path_ = std::move(path);    //set the path_
}

/**
 * Config class singleton instance getter method
 *
 * @return Config instance
 *
 * @author Michele Crepaldi s269551
 */
std::shared_ptr<server::Config> server::Config::getInstance() {
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
server::Config::Config() {
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
template <size_t Rows>
void addComments(std::fstream &file, const std::string (&comments)[Rows])
{
    for (size_t i = 0; i < Rows; ++i)   //for each comment in the comments array
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
template <size_t Rows>
void addVariables(std::fstream &file, const std::string (&variables)[Rows][3])
{
    for (size_t i = 0; i < Rows; ++i){  //for each variable in the variables array
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
 *  <b>justCreated</b> in case the file did not exist and a new one was just created (some variables are host
 *  dependant, for these variables there are no default values; the user has to modify the file and restart the program).
 * @throws ConfigException:
 *  <b>open</b> in case the config file could not be opened.
 *
 * @author Michele Crepaldi s269551
 */
void server::Config::_load() {
    std::fstream file;
    std::string str, key, value;
    std::smatch m;  //regex match

    //regex to format the strings got from file (get the key and value, ignoring comments and spaces)
    std::regex eKeyValue (R"(\s*(\w+)\s*=\s*(.+[/\w])\s*)");
    //regex to check if the value provided is actually a positive integer number
    std::regex eUint (R"(^(\d+)$)");

    //if the file is not there then create (and populate with defaults) it
    if(!std::filesystem::exists(path_)){
        file.open(path_, std::ios::out);   //create file

        Message::print(std::cout, "WARNING", "Configuration file does not exist",
                       "it will now be created with default values");

        //all host dependant variables
        std::string hostVariables[][3] = {  {"server_base_path", SERVER_PATH,
                                                "# Server base folder path (where user files will be saved)"},

                                            {"temp_path",        TEMP_PATH,
                                                "# Temporary folder path for temporary files"}};

        //all default-able variables
        std::string variables[][3] = {  {"password_database_path",  PASSWORD_DATABASE_PATH,
                                            "# Password Database path"},

                                        {"server_database_path",    DATABASE_PATH,
                                            "# Server Database path"},

                                        {"certificate_path",        CERTIFICATE_PATH,
                                            "# Server Certificate path"},

                                        {"private_key_path",        PRIVATEKEY_PATH,
                                            "# Server Private Key path"},

                                        {"ca_file_path",            CA_FILE_PATH,
                                            "# CA to use for server certificate verification"},

                                        {"listen_queue",            std::to_string(LISTEN_QUEUE),
                                            "# Size of the accept listen queue"},

                                        {"n_threads",               std::to_string(N_THREADS),
                                            "# Number of single server threads (apart from the accepting thread)"},

                                        {"socket_queue_size",       std::to_string(SOCKET_QUEUE_SIZE),
                                            "# Maximum socket queue size"},

                                        {"select_timeout_seconds",  std::to_string(SELECT_TIMEOUT_SECONDS),
                                            "# Seconds the client will wait between 2 subsequent selects on the socket"},

                                        {"timeout_seconds",         std::to_string(TIMEOUT_SECONDS),
                                            "# Seconds the server will wait before disconnecting client"},

                                        {"tmp_file_name_size",      std::to_string(TEMP_FILE_NAME_SIZE),
                                            "# Temporary files name size"},

                                        {"max_data_chunk_size",     std::to_string(MAX_DATA_CHUNK_SIZE),
                                            "# Maximum size (in bytes) of the file transfer chunks ('data' part of DATA"
                                            "messages)\n"
                                            "# the maximum size for a protocol buffer message is 64MB"
                                            "(for a TCP socket it is 1GB) \n"
                                            "# so keep it less than that"
                                            "(keeping in mind that there are also other fields in the message)"}};

        //comments on top of the file
        std::string initial_comments[] = {          "###########################################################################",
                                                    "#                                                                         #",
                                                    "#        -Configuration file for the SERVER of PDS_Backup project-        #",
                                                    "#                   (rows preceded by '#' are comments)                   #",
                                                    "#                                                                         #",
                                                    "###########################################################################"};

        //comments before the host dependant variables
        std::string host_variables_comments[] = {   "###########################################################################",
                                                    "#         Host specific variables: no default values are provided         #",
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
            if(std::regex_match(str,m,eKeyValue)){  //regex match
                //get the key and the value
                key = m[1];
                value = m[2];

                std::transform(key.begin(),key.end(),key.begin(), ::tolower);   //convert all characters to lower case

                /*
                 * +---------------------------------------------------------------------------------------------------+
                 * folder variables
                 */
                if(key == "server_base_path"){
                    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
                    //(windows vs. unix)
                    value = std::regex_replace(value, std::regex(R"(\\)"), "/");

                    //delete the tailing "/" that may be there at the end of the path (since this is a folder path)
                    _server_base_path = std::regex_replace(value, std::regex(R"(\/$)"), "");
                }
                else if(key == "temp_path"){
                    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
                    //(windows vs. unix)
                    value = std::regex_replace(value, std::regex(R"(\\)"), "/");

                    //delete the tailing "/" that may be there at the end of the path (since this is a folder path)
                    _temp_path = std::regex_replace(value, std::regex(R"(\/$)"), "");
                }


                /*
                 * +---------------------------------------------------------------------------------------------------+
                 * file variables
                 */
                else if(key == "password_database_path") {
                    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
                    //(windows vs. unix)
                    _password_database_path = std::regex_replace(value, std::regex(R"(\\)"), "/");
                }
                else if(key == "server_database_path") {
                    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
                    //(windows vs. unix)
                    _server_database_path = std::regex_replace(value, std::regex(R"(\\)"), "/");
                }
                else if(key == "certificate_path") {
                    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
                    //(windows vs. unix)
                    _certificate_path = std::regex_replace(value, std::regex(R"(\\)"), "/");
                }
                else if(key == "private_key_path") {
                    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
                    //(windows vs. unix)
                    _private_key_path = std::regex_replace(value, std::regex(R"(\\)"), "/");
                }
                else if(key == "ca_file_path") {
                    //replace all "\" (backward slashes) with "/" (slashes) in case of a different path representation
                    //(windows vs. unix)
                    _ca_file_path = std::regex_replace(value, std::regex(R"(\\)"), "/");
                }

                /*
                 * +---------------------------------------------------------------------------------------------------+
                 * other variables (all positive integers)
                 */
                else {
                    //if the value is not a positive integer number just skip it
                    if (!std::regex_match(str, m, eUint))
                        continue;

                    //convert the string representation of the integer to an unsigned long
                    //(there is no stoui for unsigned integers)

                    unsigned long l = stoul(value);
                    if (l > UINT32_MAX)  //check if the value provided fits in an unsigned integer
                        continue;   //if not just continue with the next key-value pair

                    //assign it to the corresponding unsigned integer member variable
                    if (key == "listen_queue")
                        _listen_queue = static_cast<unsigned int>(l);
                    else if (key == "n_threads")
                        _n_threads = static_cast<unsigned int>(l);
                    else if (key == "socket_queue_size")
                        _socket_queue_size = static_cast<unsigned int>(l);
                    else if (key == "select_timeout_seconds")
                        _select_timeout_seconds = static_cast<unsigned int>(l);
                    else if (key == "timeout_seconds")
                        _timeout_seconds = static_cast<unsigned int>(l);
                    else if (key == "tmp_file_name_size")
                        _tmp_file_name_size = static_cast<unsigned int>(l);
                    else if (key == "max_data_chunk_size")
                        _max_data_chunk_size = static_cast<unsigned int>(l);
                }
            }
        }
        file.close();   //close file
    }
    else
        //if the file could not be opened throw exception
        throw ConfigException("Could not open configuration file", ConfigError::open);
}

/**
 * password database path getter method (if no value was provided in the config file use a default one)
 *
 * @return password database path
 *
 * @author Michele Crepaldi s269551
 */
const std::string& server::Config::getPasswordDatabasePath() {
    if(_password_database_path.empty())
        _password_database_path = PASSWORD_DATABASE_PATH;    //set to default

    //password database path will be verified by the PWD_Database class

    return _password_database_path;
}

/**
 * server database path getter method (if no value was provided in the config file use a default one)
 *
 * @return server database path
 *
 * @author Michele Crepaldi s269551
 */
const std::string& server::Config::getServerDatabasePath() {
    if(_server_database_path.empty())
        _server_database_path = DATABASE_PATH;   //set to default

    //database path will be verified by the Database class

    return _server_database_path;
}


/**
 * server base folder path getter method (HOST specific, this has no default values; so if no value was provided
 *  an exception will be thrown)
 *
 * @return server base folder path
 *
 * @throws ConfigException:
 *  <b>serverBasePath</b> in case no value was provided (there are no defaults for this variable) OR if the
 *  value provided references a non existing directory or something which is not a directory.
 *
 * @author Michele Crepaldi s269551
 */
const std::string& server::Config::getServerBasePath() {
    if(_server_base_path.empty())    //value required.. if it is empty throw exception (there is no default value for this variable)
        throw ConfigException("Server base path was not set", ConfigError::serverBasePath);   //throw an exception

    if(!std::filesystem::exists(_server_base_path))   //if the path to watch set references a non-existent folder
        throw ConfigException("Server base path does not exist", ConfigError::serverBasePath);   //throw an exception

    if(!std::filesystem::is_directory(_server_base_path))    //if the path to watch set references something which is not a folder
        throw ConfigException("Server base path is not a directory", ConfigError::serverBasePath);   //throw an exception

    return _server_base_path;
}

/**
 * temp folder path getter method (HOST specific, this has no default values; so if no value was provided
 *  an exception will be thrown)
 *
 * @return temp folder path
 *
 * @throws ConfigException:
 *  <b>tempPath</b> in case no value was provided (there are no defaults for this variable).
 *
 * @author Michele Crepaldi s269551
 */
const std::string& server::Config::getTempPath() {
    //value required.. if it is empty throw exception (there is no default value for this variable)
    if(_temp_path.empty())
        throw ConfigException("Server temporary path was not set", ConfigError::tempPath);

    //if the temporary path does not exist it will be created automatically by the server

    return _temp_path;
}

/**
 * server certificate path getter method (if no value was provided in the config file use a default one)
 *
 * @return server certificate path
 *
 * @author Michele Crepaldi s269551
 */
const std::string& server::Config::getCertificatePath() {
    if(_certificate_path.empty())
        _certificate_path = CERTIFICATE_PATH;    //set to default

    //the certificate path will be verified by the ServerSocket class

    return _certificate_path;
}

/**
 * server private key path getter method (if no value was provided in the config file use a default one)
 *
 * @return server private key
 *
 * @author Michele Crepaldi s269551
 */
const std::string& server::Config::getPrivateKeyPath() {
    if(_private_key_path.empty())
        _private_key_path = PRIVATEKEY_PATH; //set to default

    //the private key path will be verified by the ServerSocket class

    return _private_key_path;
}

/**
 * CA file path getter method (if no value was provided in the config file use a default one)
 *
 * @return CA file path
 *
 * @author Michele Crepaldi s269551
 */
const std::string& server::Config::getCaFilePath() {
    if(_ca_file_path.empty())
        _ca_file_path = CA_FILE_PATH;    //set to default

    //the CA file path will be verified by the Socket class

    return _ca_file_path;
}

/**
 * listen queue size getter method (if no value was provided in the config file use a default one)
 *
 * @return listen queue size
 *
 * @author Michele Crepaldi s269551
 */
unsigned int server::Config::getListenQueue() {
    if(_listen_queue == 0)
        _listen_queue = LISTEN_QUEUE;    //set to default

    return _listen_queue;
}

/**
 * number of threads getter method (if no value was provided in the config file use a default one)
 *
 * @return number of threads
 *
 * @author Michele Crepaldi s269551
 */
unsigned int server::Config::getNThreads() {
    if(_n_threads == 0)
        _n_threads = N_THREADS;

    return _n_threads;
}

/**
 * socket queue size getter method (if no value was provided in the config file use a default one)
 *
 * @return socket queue size
 *
 * @author Michele Crepaldi s269551
 */
unsigned int server::Config::getSocketQueueSize() {
    if(_socket_queue_size == 0)
        _socket_queue_size = SOCKET_QUEUE_SIZE;

    return _socket_queue_size;
}

/**
 * select timeout seconds getter method (if no value was provided in the config file use a default one)
 *
 * @return select timeout seconds
 *
 * @author Michele Crepaldi s269551
 */
unsigned int server::Config::getSelectTimeoutSeconds() {
    if(_select_timeout_seconds == 0)
        _select_timeout_seconds = SELECT_TIMEOUT_SECONDS;

    return _select_timeout_seconds;
}

/**
 * timeout seconds getter method (if no value was provided in the config file use a default one)
 *
 * @return timeout seconds
 *
 * @author Michele Crepaldi s269551
 */
unsigned int server::Config::getTimeoutSeconds() {
    if(_timeout_seconds == 0)
        _timeout_seconds = TIMEOUT_SECONDS;

    return _timeout_seconds;
}

/**
 * temp file name size getter method (if no value was provided in the config file use a default one)
 *
 * @return temp file name size
 *
 * @author Michele Crepaldi s269551
 */
unsigned int server::Config::getTmpFileNameSize() {
    if(_tmp_file_name_size == 0)
        _tmp_file_name_size = TEMP_FILE_NAME_SIZE;

    return _tmp_file_name_size;
}

/**
 * max data chunk size getter method (if no value was provided in the config file use a default one)
 *
 * @return max data chunk size
 *
 * @author Michele Crepaldi s269551
 */
unsigned int server::Config::getMaxDataChunkSize() {
    if(_max_data_chunk_size == 0)
        _max_data_chunk_size = MAX_DATA_CHUNK_SIZE;

    return _max_data_chunk_size;
}