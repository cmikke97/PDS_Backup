//
// Created by michele on 29/09/2020.
//

#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <string>
#include <fstream>
#include <mutex>

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Config class
 */

namespace client {
    /**
     * Config class; used to retrive the configuration for the execution of the program from file (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class Config {
        std::string path_to_watch;
        std::string database_path;
        std::string ca_file_path;
        int millis_filesystem_watcher{};
        int event_queue_size{};
        int seconds_between_reconnections{};
        int max_connection_retries{};
        int max_server_error_retries{};
        int max_auth_error_retries{};
        int timeout_seconds{};
        int select_timeout_seconds{};
        int max_response_waiting{};
        int max_data_chunk_size{};

        void load(const std::string &configFilePath);

    protected:
        explicit Config(std::string path);

        //to sincronize threads during the first creation of the Singleton object
        static std::mutex mutex_;
        //singleton instance
        static std::shared_ptr<Config> config_;
        std::string path_;

    public:
        Config(Config *other) = delete;

        void operator=(const Config &) = delete;

        static std::shared_ptr<Config> getInstance(const std::string &path);

        std::string& getPathToWatch();

        std::string& getDatabasePath();

        std::string& getCAFilePath();

        int getMillisFilesystemWatcher();

        int getEventQueueSize();

        int getSecondsBetweenReconnections();

        int getMaxConnectionRetries();

        int getMaxServerErrorRetries();

        int getMaxAuthErrorRetries();

        int getTimeoutSeconds();

        int getSelectTimeoutSeconds();

        int getMaxResponseWaiting();

        int getMaxDataChunkSize();
    };

    /*
     * +-------------------------------------------------------------------------------------------------------------------+
     * ConfigException class
     */

    /**
     * configError class: it describes (enumerically) all the possible config errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class configError {
        open, pathToWatch, justCreated
    };

    /**
     * exceptions for the config class
     *
     * @author Michele Crepaldi s269551
     */
    class ConfigException : public std::runtime_error {
        configError code;
    public:

        /**
         * config exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        explicit ConfigException(const std::string &msg, configError code) :
                std::runtime_error(msg), code(code) {
        }

        /**
         * config exception destructor.
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
        configError getCode() const noexcept {
            return code;
        }
    };
}

#endif //CLIENT_CONFIG_H
