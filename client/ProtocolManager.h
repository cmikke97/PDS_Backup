//
// Created by Michele Crepaldi s269551 on 26/09/2020
// Finished on 26/11/2020
// Last checked on 26/11/2020
//


#ifndef CLIENT_PROTOCOLMANAGER_H
#define CLIENT_PROTOCOLMANAGER_H

#include "../myLibraries/Socket.h"
#include "../myLibraries/Directory_entry.h"
#include "../myLibraries/Circular_vector.h"
#include "../Event.h"
#include <messages.pb.h>


/**
 * PDS_Backup client namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace client {
    /**
     * ProtocolManagerError class: it describes (enumerically) all the possible ProtocolManager errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class ProtocolManagerError {
        //current user authentication failed
        auth,

        //internal server error (Fatal error)
        internal,

        //client error
        client,

        //server message error
        serverMessage,

        //server uses a different version
        version,

        //server message of an unexpected type
        unexpected,

        //unexpected ok/error code in server message
        unexpectedCode,
    };

    /**
     * ErrCode class: it describes (enumerically) all the possible message error codes
     *
     * @author Michele Crepaldi s269551
     */
    enum class ErrCode{
        //error in PROB -> the element is not a file
        notAFile,

        //unexpected message type
        unexpected,

        //error in STOR -> the written file is different from what expected (different size, hash or lastWriteTime)
        store,

        //error in DELE -> the element is not a file or the hash does not correspond
        remove,

        //error in RMD/MKD -> the element is not a directory
        notADir,

        //authentication error
        auth,

        //server exception
        exception,

        //there was an error in the client message received (such as all to false and mac address not initialized)
        retrieve
    };

    /**
     * OkCode class: it describes (enumerically) all the possible message ok codes
     *
     * @author Michele Crepaldi s269551
     */
    enum class OkCode {
        //file found
        found,

        //file/directory successfully created
        created,

        //file/directory remove did not find the file/directory (the end effect is the same as a successful remove)
        notThere,

        //file/directory successfully removed
        removed,

        //user successfully authenticated
        authenticated,

        //all the user requested data was sent
        retrieved
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * ProtocolManager class
     */

    /**
     * ProtocolManager class. It manages the communication with the server, composing messages from the events
     *  passed from main and sending those messages to the server (and reacting to the server responses).
     *  It also has a list of sent messages without response in order to be able to re-send them
     *  (so without missing events) in case of errors.
     *
     * @author Michele Crepaldi s269551
     */
    class ProtocolManager {
    public:
        ProtocolManager(const ProtocolManager &) = delete;              //copy constructor deleted
        ProtocolManager& operator=(const ProtocolManager &) = delete;   //copy assignment deleted
        ProtocolManager(ProtocolManager &&) = delete;                   //move constructor deleted
        ProtocolManager& operator=(ProtocolManager &&) = delete;        //move assignment deleted
        ~ProtocolManager() = default;   //default destructor

        //constructor with the socket and protocol version
        ProtocolManager(Socket &s, int ver);

        //authenticate method with username, password and mac address
        void authenticate(const std::string &username, const std::string &password, const std::string &macAddress);

        bool canSend() const;       //boolean "can it send event messages" method
        bool isWaiting() const;     //boolean "is waiting for responses" method
        int nWaiting() const;       //number of event messages waiting for responses getter method

        void recoverFromError();    //recover from error method
        void send(Event &event);    //send event message to server method
        void receive();             //receive response message from server method

        //retrieve the files from server method (with mac, all boolean and destination folder)
        void retrieveFiles(const std::string &macAddress, bool all, const std::string &destFolder);

    private:
        Socket &_s; //socket associated to the protocol manager

        std::shared_ptr<Database> _db;  //shared pointer to the Database object

        messages::ClientMessage _clientMessage; //protocol buffer message to use to get messages from client
        messages::ServerMessage _serverMessage; //protocol buffer message to use to reply to client

        std::string _path_to_watch; //path watched by the client (the same as used by FileSystemWatcher)

        Circular_vector<Event> _waitingForResponse; //event messages waiting for a response queue (circular vector)

        int _protocolVersion;   //client's protocol version

        unsigned int _tries;            //current number of retries //TODO check if it needed
        unsigned int _maxTries;         //max number of retries upon server error   //TODO check if it is needed
        unsigned int _tempNameSize;     //size of the temporary files name
        unsigned int _maxDataChunkSize; //maximum size of sent data chunk

        void _send_clientMessage();     //send clientMessage method

        /*
         * +-----------------------------------------------------------------------------------------------------------+
         * methods for the normal protocol manager usage: client -> server data transfer
         */

        //send message methods for the normal usage

        //send AUTH message method
        void _send_AUTH(const std::string &username, const std::string &macAddress, const std::string &password);

        void _send_PROB(Directory_entry &e);        //send PROB message method
        void _send_DELE(Directory_entry &e);        //send DELE message method
        void _send_STOR(Directory_entry &e);        //send STOR message method
        void _send_DATA(char *buff, uint64_t len);  //send DATA message method
        void _send_MKD(Directory_entry &e);         //send MKD message method
        void _send_RMD(Directory_entry &e);         //send RMD message method

        //client action performing methods
        void _composeMessage(Event &event);             //compose message method
        void _sendFile(Directory_entry &element);   //send file method

        /*
         * +-----------------------------------------------------------------------------------------------------------+
         * methods for the special case of protocol manager usage: server -> client data transfer
         */

        //send message methods for the special case of client retrieving data from server
        void _send_RETR(const std::string &macAddress, bool all);   //send RETR message method

        //special action performing methods for the special case of client retrieving data from server
        void _storeFile(const std::string &destFolder, const std::string &temporaryPath);   //store file method
        void _makeDir(const std::string &destFolder);   //create folder method
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * ProtocolManagerException class
     */

    /**
     * ProtocolManagerException exception class that may be returned by the ProtocolManager class
     *  (derives from runtime_error)
     *
     * @author Michele Crepaldi s269551
     */
    class ProtocolManagerException : public std::runtime_error {
    public:

        /**
         * protocol manager exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        ProtocolManagerException(const std::string &msg, ProtocolManagerError errorCode) :
                std::runtime_error(msg), _code(errorCode) {
        }

        /**
         * protocol manager exception destructor
         *
         * @author Michele Crepaldi s269551
         */
        ~ProtocolManagerException() noexcept override = default;

        /**
         * function to retrieve the error code from the exception
         *
         * @return protocol manager error code
         *
         * @author Michele Crepaldi s269551
         */
        ProtocolManagerError getErrorCode() const noexcept {
            return _code;
        }

    private:
        ProtocolManagerError _code; //code describing the error
    };
}

#endif //CLIENT_PROTOCOLMANAGER_H
