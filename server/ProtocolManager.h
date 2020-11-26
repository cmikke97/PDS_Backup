//
// Created by Michele Crepaldi s269551 on 21/10/2020
// Finished on 22/11/2020
// Last checked on 22/11/2020
//


#ifndef SERVER_PROTOCOLMANAGER_H
#define SERVER_PROTOCOLMANAGER_H

#include "../myLibraries/Socket.h"
#include "../myLibraries/Directory_entry.h"
#include "messages.pb.h"
#include "Database.h"
#include "Database_pwd.h"


/**
 * PDS_Backup server namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace server {
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

        //error in client message
        client,

        //current client uses a different version
        version,

        //client message of an unexpected type
        unexpected
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
     * ProtocolManager class. It manages the communication with the client interpreting the messages,
     *  executing the related actions and responding back to the client.
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

        //constructor with the socket, client address, protocol version
        ProtocolManager(Socket &s, std::string address, int ver);

        void recoverFromDB();   //recover from database method
        void authenticate();    //authenticate method
        void receive();         //receive message from client method

    private:
        Socket &_s;  //socket associated to the protocol manager

        std::shared_ptr<Database> _db;              //shared pointer to the Database object
        std::shared_ptr<Database_pwd> _password_db; //shared pointer to the Database_pwd object

        messages::ClientMessage _clientMessage; //protocol buffer message to use to get messages from client
        messages::ServerMessage _serverMessage; //protocol buffer message to use to reply to client

        std::string _username;      //username of the connected user
        std::string _mac;           //mac address of the connected client's machine
        std::string _address;       //address of the connected client's machine
        std::string _basePath;      //base server path (where to put backed-up data)
        std::string _temporaryPath; //temporary server path where to put temporary files

        int _protocolVersion;       //server's protocol version

        unsigned int _tempNameSize;          //size of the temporary files name
        unsigned int _maxDataChunkSize;      //maximum size of sent data chunk

        bool _recovered;    //whether the protocol manager already recovered data from database or not

        //map of saved directory entries for this username-mac
        std::unordered_map<std::string, Directory_entry> _elements;

        void _send_serverMessage(); //send serverMessage method

        /*
         * +-----------------------------------------------------------------------------------------------------------+
         * methods for the normal protocol manager usage: client -> server data transfer
         */

        //send message methods for the normal usage
        void _send_OK(OkCode code);     //send OK message method
        void _send_SEND(const std::string &path, const std::string &hash);  //send SEND message method
        void _send_ERR(ErrCode code);   //send ERR message method
        void _send_VER();               //send VER message method

        //server action performing methods
        void _probe();      //probe file method
        void _storeFile();  //store file method
        void _removeFile(); //remove file method
        void _makeDir();    //make directory method
        void _removeDir();  //remove directory method

        /*
         * +-----------------------------------------------------------------------------------------------------------+
         * methods for the special case of protocol manager usage: server -> client data transfer
         */

        //send message methods for the special case of client retrieving data from server
        void _send_MKD(const std::string &path, Directory_entry &element);  //send MKD message method
        void _send_STOR(const std::string &path, Directory_entry &element); //send STOR message method
        void _send_DATA(char *buff, uint64_t len);                      //send DATA message method

        //special action performing methods for the special case of client retrieving data from server
        void _retrieveUserData();   //user data retrieve method
        void _sendFile(Directory_entry &element, std::string &macAddr); //send file to client method
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
        ProtocolManagerException(const std::string &msg, ProtocolManagerError code) :
                std::runtime_error(msg), _code(code) {
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
        ProtocolManagerError getCode() const noexcept {
            return _code;
        }

    private:
        ProtocolManagerError _code;  //code describing the error
    };
}

#endif //SERVER_PROTOCOLMANAGER_H
