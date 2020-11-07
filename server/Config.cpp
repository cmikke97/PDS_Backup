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
#define TIMEOUT_SECONDS 60     //Seconds the server will wait before disconnecting client
#define PASSWORD_DATABASE_PATH "../serverFiles/passwordDB.sqlite" //Password database path
#define DATABASE_PATH "../serverFiles/serverDB.sqlite"    //Server Databse path
#define SERVER_PATH "C:/Users/michele/Desktop/server_folder"    //Server base path
#define TEMP_PATH "C:/Users/michele/Desktop/server_folder/temp" //Temporary path where to put temporary files
#define TEMP_FILE_NAME_SIZE 8   //Size of the name of temporary files
#define CERTIFICATE_PATH "../../TLScerts/server_cert.pem"   //Server certificate path
#define PRIVATEKEY_PATH "../../TLScerts/server_pkey.pem"    //Server private key path
#define CA_FILE_PATH "../../TLScerts/cacert.pem"    //CA to use for server certificate verification

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
void server::Config::load(const std::string &configFilePath) {
    std::fstream file;
    std::string str, key, value;
    std::smatch m;
    std::regex e (R"(\s*(\w+)\s*=\s*(.+[/\w])\s*)");

    //if the file is not there then create (and populate with defaults) it
    if(!std::filesystem::directory_entry(configFilePath).exists()){
        file.open(configFilePath, std::ios::out);

        TS_Message::print(std::cout, "WARNING", "Configuration file does not exist", "it will now be created with default values");

        std::string variables[13][3] = {{"password_database_path",  PASSWORD_DATABASE_PATH,                 "# Password database path"},
                                        {"server_database_path",    DATABASE_PATH,                          "# Server Databse path"},
                                        {"server_base_path",        SERVER_PATH,                            "# Server base path"},
                                        {"temp_path",               TEMP_PATH,                              "# Temporary path where to put temporary files"},
                                        {"certificate_path",        CERTIFICATE_PATH,                       "# Server certificate path"},
                                        {"private_key_path",        PRIVATEKEY_PATH,                        "# Server private key path"},
                                        {"ca_file_path",            CA_FILE_PATH,                           "# CA to use for server certificate verification"},
                                        {"listen_queue",            std::to_string(LISTEN_QUEUE),           "# Size of the accept listen queue"},
                                        {"n_threads",               std::to_string(N_THREADS),              "# Number of single server threads (apart from the accepting thread)"},
                                        {"socket_queue_size",       std::to_string(SOCKET_QUEUE_SIZE),      "# Maximum socket queue size"},
                                        {"select_timeout_seconds",  std::to_string(SELECT_TIMEOUT_SECONDS), "# Seconds the client will wait between 2 selects on the socket"},
                                        {"timeout_seconds",         std::to_string(TIMEOUT_SECONDS),        "# Seconds the server will wait before disconnecting client"},
                                        {"tmp_file_name_size",      std::to_string(TEMP_FILE_NAME_SIZE),    "# Size of the name of temporary files"}};

        std::string initial_comments[] = {  "###########################################################################",
                                            "#                                                                         #",
                                            "#         Configuration file for the server of PDS_Backup project         #",
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
 * server base path getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getServerBasePath() {
    if(server_base_path.empty())
        server_base_path = SERVER_PATH;

    //if the server base path does not exist it will be created automatically by the server

    return server_base_path;
}

/**
 * temp path getter (if no value was provided in the config file use a default one)
 *
 * @return path to watch
 *
 * @author Michele Crepaldi s269551
 */
const std::string &server::Config::getTempPath() {
    if(temp_path.empty())
        temp_path = TEMP_PATH;

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
