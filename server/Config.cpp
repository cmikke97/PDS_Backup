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
#define LISTEN_QUEUE 8  //Size of the accept listen queue
#define N_THREADS 4     //Number of single server threads (apart from the accepting thread)
#define SOCKET_QUEUE_SIZE 10    //Maximum socket queue size
#define SELECT_TIMEOUT_SECONDS 5    //Seconds the client will wait between 2 selects on the socket
#define TIMEOUT_SECONDS 30     //Seconds the server will wait before disconnecting client
#define PASSWORD_DATABASE_PATH "../serverFiles/passwordDB.sqlite" //Password database path
#define DATABASE_PATH "../serverFiles/serverDB.sqlite"    //Server Databse path
#define TEMP_FILE_NAME_SIZE 8   //Size of the name of temporary files
#define CERTIFICATE_PATH "../../TLScerts/server_cert.pem"   //Server certificate path
#define PRIVATEKEY_PATH "../../TLScerts/server_pkey.pem"    //Server private key path
#define CA_FILE_PATH "../../TLScerts/cacert.pem"    //CA to use for server certificate verification

//host dependant values (variables which depend on the specific host/machine which the program is running on)
//so their default value is the empty string meaning that the user will be asked to properly specify them
#define SERVER_PATH ""    //Server base path
#define TEMP_PATH "" //Temporary path where to put temporary files


//static variables definition
std::shared_ptr<server::Config> server::Config::config_;
std::mutex server::Config::mutex_;

/**
 * Config class singleton instance getter
 *
 * @param path path of the config file on disk
 * @return Config instance
 *
 * @author Michele Crepaldi s269551
 */
std::shared_ptr<server::Config> server::Config::getInstance(const std::string &path) {
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
server::Config::Config(std::string path) : path_(std::move(path)) {
    load(path_);
}

/**
 * function used to add a single configuration variable line to the config file
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
 * function used to add a single comment line to the config file
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
 * function to add an array of comments to the config file
 *
 * @tparam rows number of rows of the comments array
 * @param file config file fstream to add the lines to
 * @param comments reference to the comments array to insert into the config file
 *
 * @author Michele Crepaldi s269551
 */
template <size_t rows>
void addComments(std::fstream &file, const std::string (&comments)[rows])
{
    for (size_t i = 0; i < rows; ++i)
        addSingleComment(file, comments[i]);
    file << std::endl;
}

/**
 * function to add an array (matrix) of variables to the config file
 *
 * @tparam rows number of rows of the variables array
 * @param file config file fstream to add the lines to
 * @param variables reference to the variables array (matrix) to insert into the config file
 */
template <size_t rows>
void addVariables(std::fstream &file, const std::string (&variables)[rows][3])
{
    for (size_t i = 0; i < rows; ++i){
        addSingleComment(file, variables[i][2]);
        addConfigVariable(file, variables[i][0], variables[i][1]);
        file << std::endl;
    }
    file << std::endl;
}

/**
 * function to load the configuration from the file (it creates it with default values if it does not exist)
 *
 * @param configFilePath path of the file to load configuration from
 *
 * @author Michele Crepaldi s269551
 */
void server::Config::load(const std::string &configFilePath) {
    std::fstream file;
    std::string str, key, value;
    std::smatch m;
    std::regex e (R"(\s*(\w+)\s*=\s*(.+[/\w])\s*)");

    //if the file is not there then create (and populate with defaults) it
    if(!std::filesystem::directory_entry(configFilePath).exists()){
        file.open(configFilePath, std::ios::out);

        TS_Message::print(std::cout, "WARNING", "Configuration file does not exist", "it will now be created with default values");

        std::string hostVariables[][3] = {  {"server_base_path", SERVER_PATH, "# Server base path"},
                                            {"temp_path",        TEMP_PATH,   "# Temporary path where to put temporary files"}};

        std::string variables[][3] = {  {"password_database_path",  PASSWORD_DATABASE_PATH,                 "# Password database path"},
                                        {"server_database_path",    DATABASE_PATH,                          "# Server Databse path"},
                                        {"certificate_path",        CERTIFICATE_PATH,                       "# Server certificate path"},
                                        {"private_key_path",        PRIVATEKEY_PATH,                        "# Server private key path"},
                                        {"ca_file_path",            CA_FILE_PATH,                           "# CA to use for server certificate verification"},
                                        {"listen_queue",            std::to_string(LISTEN_QUEUE),           "# Size of the accept listen queue"},
                                        {"n_threads",               std::to_string(N_THREADS),              "# Number of single server threads (apart from the accepting thread)"},
                                        {"socket_queue_size",       std::to_string(SOCKET_QUEUE_SIZE),      "# Maximum socket queue size"},
                                        {"select_timeout_seconds",  std::to_string(SELECT_TIMEOUT_SECONDS), "# Seconds the client will wait between 2 selects on the socket"},
                                        {"timeout_seconds",         std::to_string(TIMEOUT_SECONDS),        "# Seconds the server will wait before disconnecting client"},
                                        {"tmp_file_name_size",      std::to_string(TEMP_FILE_NAME_SIZE),    "# Size of the name of temporary files"}};

        std::string initial_comments[] = {          "###########################################################################",
                                                    "#                                                                         #",
                                                    "#         Configuration file for the SERVER of PDS_Backup project         #",
                                                    "#                   (rows preceded by '#' are comments)                   #",
                                                    "#                                                                         #",
                                                    "###########################################################################"};

        std::string host_variables_comments[] = {   "###########################################################################",
                                                    "#         Host specific variables: no default values are provided         #",
                                                    "###########################################################################"};

        std::string variables_comments[] = {        "###########################################################################",
                                                    "#                             Other variables                             #",
                                                    "#        -  in case of empty fields default values will be used  -        #",
                                                    "###########################################################################"};

        std::string final_comments[]  = {           "###########################################################################",
                                                    "#                                                                         #",
                                                    "#        -              Configuration file finished              -        #",
                                                    "#                                                                         #",
                                                    "###########################################################################"};

        //add initial comments
        addComments(file, initial_comments);

        //add host variables comments
        addComments(file, host_variables_comments);

        //add host variables
        addVariables(file, hostVariables);

        //add variables comments
        addComments(file, variables_comments);

        //add other variables
        addVariables(file, variables);

        //add final comments
        addComments(file, final_comments);

        file.close();

        throw ConfigException("Configuration file created, modify it and restart.", configError::justCreated);
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
                if(key == "server_base_path"){
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    value = std::regex_replace(value, std::regex("\\\\"), "/");

                    //delete the tailing / that may be there at the end of the path
                    std::smatch mtmp;
                    std::regex etmp ("(.*)\\/$");
                    if(std::regex_match (value,m,e))    //if i fine the trailing "/" i remove it
                        server_base_path = m[1];
                    else
                        server_base_path = value;
                }
                if(key == "temp_path"){
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    value = std::regex_replace(value, std::regex("\\\\"), "/");

                    //delete the tailing / that may be there at the end of the path
                    std::smatch mtmp;
                    std::regex etmp ("(.*)\\/$");
                    if(std::regex_match (value,m,e))    //if i fine the trailing "/" i remove it
                        temp_path = m[1];
                    else
                        temp_path = value;
                }

                //files
                if(key == "password_database_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    password_database_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }
                if(key == "server_database_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    server_database_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }
                if(key == "certificate_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    certificate_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }
                if(key == "private_key_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    private_key_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }
                if(key == "ca_file_path") {
                    //replace all \ (backward slashes) to / (slashes) in case of a different path representation
                    ca_file_path = std::regex_replace(value, std::regex("\\\\"), "/");
                }

                //integers
                if(key == "listen_queue")
                    listen_queue = stoi(value);
                if(key == "n_threads")
                    n_threads = stoi(value);
                if(key == "socket_queue_size")
                    socket_queue_size = stoi(value);
                if(key == "select_timeout_seconds")
                    select_timeout_seconds = stoi(value);
                if(key == "timeout_seconds")
                    timeout_seconds = stoi(value);
                if(key == "tmp_file_name_size")
                    tmp_file_name_size = stoi(value);
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
 * password database path getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getPasswordDatabasePath() {
    if(password_database_path.empty())
        password_database_path = PASSWORD_DATABASE_PATH;

    //password database path will be verified by the PWD_Database class

    return password_database_path;
}

/**
 * server database path getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getServerDatabasePath() {
    if(server_base_path.empty())
        server_database_path = DATABASE_PATH;

    //database path will be verified by the Database class

    return server_database_path;
}

/**
 * server base path getter (HOST specific, this has no default values; so if no value was provided an exception will be thrown)
 *
 * @return path to watch
 *
 * @throw ConfigException in case no value was provided (there are no defaults for this variable) OR in case the value provided
 * references a non existing directory OR something which is not a directory
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getServerBasePath() {

    if(server_base_path.empty())    //value required.. if it is empty throw exception (there is no default value for this variable)
        throw ConfigException("Server base path was not set", configError::serverBasePath);   //throw an exception

    //if the path to watch set references a non-existant folder
    if(!std::filesystem::exists(server_base_path))
        throw ConfigException("Server base path does not exist", configError::serverBasePath);   //throw an exception

    //if the path to watch set references something which is not a folder
    if(!std::filesystem::is_directory(server_base_path))
        throw ConfigException("Server base path is not a directory", configError::serverBasePath);   //throw an exception

    return server_base_path;
}

/**
 * temp path getter (HOST specific, this has no default values; so if no value was provided an exception will be thrown)
 *
 * @return path to watch
 *
 * @throw ConfigException in case no value was provided (there are no defaults for this variable)
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getTempPath() {

    if(temp_path.empty())    //value required.. if it is empty throw exception (there is no default value for this variable)
        throw ConfigException("Server temporary path was not set", configError::tempPath);   //throw an exception

    //if the temporary path does not exist it will be created automatically by the server

    return temp_path;
}

/**
 * server certificate path getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getCertificatePath() {
    if(certificate_path.empty())
        certificate_path = CERTIFICATE_PATH;

    //the certificate path will be verified by the ServerSocket class

    return certificate_path;
}

/**
 * server private key path getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getPrivateKeyPath() {
    if(private_key_path.empty())
        private_key_path = PRIVATEKEY_PATH;

    //the private key path will be verified by the ServerSocket class

    return private_key_path;
}

/**
 * CA file path getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getCaFilePath() {
    if(ca_file_path.empty())
        ca_file_path = CA_FILE_PATH;

    //the CA file path will be verified by the Socket class

    return ca_file_path;
}

/**
 * listen queue size getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
int server::Config::getListenQueue() {
    if(listen_queue == 0)
        listen_queue = LISTEN_QUEUE;

    return listen_queue;
}

/**
 * # of threads getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
int server::Config::getNThreads() {
    if(n_threads == 0)
        n_threads = N_THREADS;

    return n_threads;
}

/**
 * socket queue size getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
int server::Config::getSocketQueueSize() {
    if(socket_queue_size == 0)
        socket_queue_size = SOCKET_QUEUE_SIZE;

    return socket_queue_size;
}

/**
 * select timeout seconds getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
int server::Config::getSelectTimeoutSeconds() {
    if(select_timeout_seconds == 0)
        select_timeout_seconds = SELECT_TIMEOUT_SECONDS;

    return select_timeout_seconds;
}

/**
 * timeout seconds getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
int server::Config::getTimeoutSeconds() {
    if(timeout_seconds == 0)
        timeout_seconds = TIMEOUT_SECONDS;

    return timeout_seconds;
}

/**
 * temp file name size getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
int server::Config::getTmpFileNameSize() {
    if(tmp_file_name_size == 0)
        tmp_file_name_size = TEMP_FILE_NAME_SIZE;

    return tmp_file_name_size;
}
