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
        int code, data;

    public:

        /**
         * protocol manager exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        ProtocolManagerException(const std::string &msg, int errorCode, int data) :
                std::runtime_error(msg), code(errorCode), data(data) {
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
        int getErrorCode() const noexcept {
            return code;
        }

        /**
         * function to retrieve the data associated to the error code from the exception
         *
         * @return data
         *
         * @author Michele Crepaldi s269551
         */
        int getData() const noexcept {
            return data;
        }
    };
}

#endif //CLIENT_PROTOCOLMANAGER_H
