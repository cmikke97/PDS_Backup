//
// Created by michele on 21/10/2020.
//

#ifndef SERVER_PROTOCOLMANAGER_H
#define SERVER_PROTOCOLMANAGER_H


#include "../myLibraries/Socket.h"
#include "../myLibraries/Directory_entry.h"
#include "messages.pb.h"
#include "Database.h"
#include "PWD_Database.h"

namespace server {

    enum class protocolManagerError {
        unknown, auth, internal, version, unsupported
    };

    class ProtocolManager {
        Socket &s;
        std::shared_ptr<Database> db;
        std::shared_ptr<PWD_Database> password_db;
        messages::ClientMessage clientMessage;
        messages::ServerMessage serverMessage;
        std::string username, mac, basePath, temporaryPath;
        int tempNameSize, protocolVersion;

        std::unordered_map<std::string, Directory_entry> elements;
        //TODO i have a database of directory entries (also with info about their owner username and mac);
        //TODO so these elements will be the ones related to the username and mac of this instance of the pm (we know them after authentication)

        void send_OK(int code);
        void send_SEND();
        void send_ERR(protocolManagerError code);
        void send_VER();
        void errorHandler(const std::string &msg, protocolManagerError code);
        void probe();
        void storeFile();
        void removeFile();
        void makeDir();
        void removeDir();
        void quit();


    public:
        explicit ProtocolManager(Socket &s, int ver, std::string basePath, std::string tempPath, int tempSize);
        void recoverFromDB();
        void authenticate();
        void receive();

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

#endif //SERVER_PROTOCOLMANAGER_H
