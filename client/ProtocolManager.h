//
// Created by michele on 26/09/2020.
//

#ifndef CLIENT_PROTOCOLMANAGER_H
#define CLIENT_PROTOCOLMANAGER_H


#include <string>
#include <messages.pb.h>
#include "../myLibraries/Socket.h"
#include "../Event.h"
#include "Config.h"

#define CONFIG_FILE_PATH "../config.txt"
#define MAXBUFFSIZE 1024

namespace client {

    /**
     * enumerator class to represetn all possible protocolManager errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class protocolManagerError {
        unknown, auth, internal, client, version, unsupported, unexpected
    };

    /**
     * enumerator class to represent all possible errorCodes
     *
     * @author Michele Crepaldi s269551
     */
    enum class errCode{
        notAFile, unexpected, store, remove, notADir, auth, exception
    };

    /**
     * enumerator class to represent all possible ok codes
     *
     * @author Michele Crepaldi s269551
     */
    enum class okCode {
        found, created, notThere, removed, authenticated
    };

    /**
     * class used to manage the interactions with the server
     *
     * @author Michele Crepaldi
     */
    class ProtocolManager {
        Socket &s;
        std::shared_ptr<Database> db;
        messages::ClientMessage clientMessage;
        messages::ServerMessage serverMessage;

        std::string path_to_watch;

        std::vector<Event> waitingForResponse{};
        int start, end, size, protocolVersion;
        int tries, maxTries;

        void send_AUTH(const std::string &username, const std::string &macAddress, const std::string &password);
        void send_QUIT();
        void send_PROB(Directory_entry &e);
        void send_DELE(Directory_entry &e);
        void send_STOR(Directory_entry &e);
        void send_MKD(Directory_entry &e);
        void send_RMD(Directory_entry &e);
        void composeMessage(Event &e);

    public:
        ProtocolManager(Socket &s, int max, int ver, int maxTries, std::string path);

        void authenticate(const std::string &username, const std::string &password, const std::string &macAddress);

        void quit();

        void send(Event &e);

        void receive();

        bool canSend() const;

        void recoverFromError();

        void sendFile(const std::filesystem::path &path);
    };

    /**
     * exceptions for the protocol manager class
     *
     * @author Michele Crepaldi s269551
     */
    class ProtocolManagerException : public std::runtime_error {
        protocolManagerError code;

    public:

        /**
         * protocol manager exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        ProtocolManagerException(const std::string &msg, protocolManagerError errorCode) :
                std::runtime_error(msg), code(errorCode) {
        }

        /**
         * protocol manager exception destructor.
         *
         * @author Michele Crepaldi s269551
         */
        ~ProtocolManagerException() noexcept override = default;

        /**
         * function to retrieve the error code from the exception
         *
         * @return error code
         *
         * @author Michele Crepaldi s269551
         */
        protocolManagerError getErrorCode() const noexcept {
            return code;
        }
    };
}

#endif //CLIENT_PROTOCOLMANAGER_H
