//
// Created by Michele Crepaldi s269551 on 29/09/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <mutex>
#include <memory>


/**
 * PDS_Backup server namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace server {
    /**
     * ConfigError class: it describes (enumerically) all the possible Config errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class ConfigError {
        //no path set
        path,

        //the config file could not be opened
        open,

        //the file did not exist and a new one was just created (some variables are host dependant,
        //for these variables there are no default values; the user has to modify the file and restart the program)
        justCreated,

        //no value was provided for this variable (there are no defaults for this variable) OR the value provided
        //references a non existing directory or something which is not a directory
        serverBasePath,

        //no value was provided for this variable (there are no defaults for this variable)
        tempPath
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * Config class
     */

    /**
     * Config class. Used to retrieve the configuration for the execution of the program from file (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class Config {
    public:
        Config(const Config &) = delete;    //copy constructor deleted
        Config& operator=(const Config &) = delete; //assignment deleted
        Config(Config &&) = delete; //move constructor deleted
        Config& operator=(Config &&) = delete;  //move assignment deleted
        ~Config() = default;

        static void setPath(std::string path);

        //singleton instance getter
        static std::shared_ptr<Config> getInstance();

        //getters

        const std::string& getPasswordDatabasePath();
        const std::string& getServerDatabasePath();
        const std::string& getServerBasePath();
        const std::string& getTempPath();
        const std::string& getCertificatePath();
        const std::string& getPrivateKeyPath();
        const std::string& getCaFilePath();
        unsigned int getListenQueue();
        unsigned int getNThreads();
        unsigned int getSocketQueueSize();
        unsigned int getSelectTimeoutSeconds();
        unsigned int getTimeoutSeconds();
        unsigned int getTmpFileNameSize();
        unsigned int getMaxDataChunkSize();

    protected:
        //protected constructor
        Config();

        //mutex to synchronize threads during the first creation of the Singleton object
        static std::mutex mutex_;

        //singleton instance
        static std::shared_ptr<Config> config_;

        //path of the config file
        static std::string path_;

    private:
        //host dependant variables

        std::string _server_base_path;
        std::string _temp_path;

        //default-able variables

        std::string _password_database_path;
        std::string _server_database_path;
        std::string _certificate_path;
        std::string _private_key_path;
        std::string _ca_file_path;
        unsigned int _listen_queue{};
        unsigned int _n_threads{};
        unsigned int _socket_queue_size{};
        unsigned int _select_timeout_seconds{};
        unsigned int _timeout_seconds{};
        unsigned int _tmp_file_name_size{};
        unsigned int _max_data_chunk_size{};

        //config file load function
        void _load();
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * ConfigException class
     */

    /**
     * ConfigException exception class that may be returned by the Config class
     *  (derives from runtime_error)
     *
     * @author Michele Crepaldi s269551
     */
    class ConfigException : public std::runtime_error {
    public:
        /**
         * config exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        explicit ConfigException(const std::string &msg, ConfigError code) :
                std::runtime_error(msg), _code(code) {
        }

        /**
         * config exception destructor
         *
         * @author Michele Crepaldi s269551
         */
        ~ConfigException() noexcept override = default;

        /**
        * function to retrieve the error code from the exception
        *
        * @returns config error code
        *
        * @author Michele Crepaldi s269551
        */
        ConfigError getCode() const noexcept {
            return _code;
        }

    private:
        ConfigError _code;   //code describing the error
    };
}

#endif //SERVER_CONFIG_H
