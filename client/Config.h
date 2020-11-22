//
// Created by Michele Crepaldi s269551 on 29/09/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <string>
#include <mutex>
#include <memory>


/**
 * PDS_Backup client namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace client {
    /**
     * configError class: it describes (enumerically) all the possible config errors
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
        pathToWatch
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * Config class
     */

    /**
     * Config class. Used to retrive the configuration for the execution of the program from file (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class Config {
    public:
        Config(Config *other) = delete; //copy constructor deleted
        Config& operator=(const Config &) = delete;    //assignment deleted
        Config(Config &&) = delete; //move constructor deleted
        Config& operator=(Config &&) = delete;  //move assignment deleted
        ~Config() = default;

        static void setPath(std::string path);

        //singleton instance getter
        static std::shared_ptr<Config> getInstance();

        //getters

        const std::string& getPathToWatch();
        const std::string& getDatabasePath();
        const std::string& getCAFilePath();
        unsigned int getMillisFilesystemWatcher();
        unsigned int getEventQueueSize();
        unsigned int getSecondsBetweenReconnections();
        unsigned int getMaxConnectionRetries();
        unsigned int getMaxServerErrorRetries();
        unsigned int getTimeoutSeconds();
        unsigned int getSelectTimeoutSeconds();
        unsigned int getMaxResponseWaiting();
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

        std::string _path_to_watch;

        //default-able variables

        std::string _database_path;
        std::string _ca_file_path;
        unsigned int _millis_filesystem_watcher{};
        unsigned int _event_queue_size{};
        unsigned int _seconds_between_reconnections{};
        unsigned int _max_connection_retries{};
        unsigned int _max_server_error_retries{};
        unsigned int _timeout_seconds{};
        unsigned int _select_timeout_seconds{};
        unsigned int _max_response_waiting{};
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
        * @return error code
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

#endif //CLIENT_CONFIG_H
