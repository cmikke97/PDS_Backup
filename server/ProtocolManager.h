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

    /**
     * enumerator class to represetn all possible protocolManager errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class protocolManagerError {
        unknown, auth, internal, client, version, unsupported
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
     * Protocol Manager class for the server. It manages the communication with the client interpreting the messages,
     * executing the related actions and responding back to the client.
     *
     * @author Michele Crepaldi s269551
     */
    class ProtocolManager {
        Socket &s;
        std::shared_ptr<Database> db;
        std::shared_ptr<PWD_Database> password_db;
        messages::ClientMessage clientMessage;
        messages::ServerMessage serverMessage;
        std::string username, mac, basePath, temporaryPath, address;
        int tempNameSize, protocolVersion;

        std::unordered_map<std::string, Directory_entry> elements;

        void send_OK(okCode code);
        void send_SEND();
        void send_ERR(errCode code);
        void send_VER();
        //void send_QUIT();
        void probe();
        void storeFile();
        void removeFile();
        void makeDir();
        void removeDir();
        //void quit();

    public:
        explicit ProtocolManager(Socket &s, const std::string &address, int ver, std::string basePath, std::string tempPath, int tempSize);
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
        ProtocolManagerException(const std::string &msg, protocolManagerError code) :
                std::runtime_error(msg), code(code) {
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
        protocolManagerError getCode() const noexcept {
            return code;
        }
    };
}

#endif //SERVER_PROTOCOLMANAGER_H
