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

namespace server {
    /**
     * Config class; used to retrive the configuration for the execution of the program from file (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class Config {
        std::string password_database_path;
        std::string server_database_path;
        std::string server_base_path;
        std::string temp_path;
        std::string certificate_path;
        std::string private_key_path;
        std::string ca_file_path;
        int listen_queue{};
        int n_threads{};
        int socket_queue_size{};
        int select_timeout_seconds{};
        int timeout_seconds{};
        int tmp_file_name_size{};

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

        const std::string &getPasswordDatabasePath();

        const std::string &getServerDatabasePath();

        const std::string &getServerBasePath();

        const std::string &getTempPath();

        const std::string &getCertificatePath();

        const std::string &getPrivateKeyPath();

        const std::string &getCaFilePath();

        int getListenQueue();

        int getNThreads();

        int getSocketQueueSize();

        int getSelectTimeoutSeconds();

        int getTimeoutSeconds();

        int getTmpFileNameSize();
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
        open, serverBasePath, tempPath, justCreated
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
