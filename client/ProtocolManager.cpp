//
// Created by Michele Crepaldi s269551 on 26/09/2020
// Finished on 26/11/2020
// Last checked on 26/11/2020
//

#include "ProtocolManager.h"

#include <fstream>

#include "../myLibraries/Message.h"
#include "../myLibraries/RandomNumberGenerator.h"
#include "../myLibraries/Validator.h"
#include "Config.h"
#include <cmath>

#define TEMP_RELATIVE_PATH "/temp"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * ProtocolManager class methods
 */

/**
 * ProtocolManager constructor
 *
 * @param socket socket associated to this thread
 * @param ver version of the protocol tu use
 *
 * @author Michele Crepaldi s269551
 */
client::ProtocolManager::ProtocolManager(Socket &socket, Circular_vector<Event> &waitingForResponse, int ver) :
        _s(socket), //set socket
        _waitingForResponse(waitingForResponse),    //set waitingForResponse object
        _protocolVersion(ver) { //set protocol version

    auto config = Config::getInstance();    //config object instance
    _path_to_watch = config->getPathToWatch();  //get path to watch
    _tempNameSize = config->getTmpFileNameSize();       //get temporary file name size
    _maxDataChunkSize = config->getMaxDataChunkSize();  //get max data chunk size

    _db = Database::getInstance();              //get database instance
}

/**
 * ProtocolManager authenticate method.
 *  It is used to authenticate the client to the server.
 *
 * @param username username of the user
 * @param password password of the user
 * @param macAddress macAddress of this user's machine
 *
 * @throws ProtocolManagerException:
 *  <b>version</b> if the server message uses a different version
 * @throws ProtocolManagerException:
 *  <b>auth</b> if the authentication failed
 * @throws ProtocolManagerException:
 *  <b>internal</b> if there has been an internal server error
 * @throws ProtocolManagerException:
 *  <b>unexpectedCode</b> if an unexpected ok/error code was present in server message
 * @throws ProtocolManagerException:
 *  <b>unexpected</b> if an unexpected server message was received
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::authenticate(const std::string& username, const std::string& password,
                                           const std::string& macAddress) {

    //send authentication message to server
    _send_AUTH(username, macAddress, password);

    std::string response = _s.recvString();     //server response
    _serverMessage.ParseFromString(response);   //get serverMessage protobuf parsing the server response

    //check serverMessage version
    if(_serverMessage.version() != _protocolVersion) {
        //it is more efficient to clear the serverMessage protobuf than creating a new one
        _serverMessage.Clear();

        throw ProtocolManagerException("Server is using a different version ",
                                       ProtocolManagerError::version);
    }

    switch (_serverMessage.type()) {
        case messages::ServerMessage_Type_OK: {
            int okCode = _serverMessage.code();   //ok code

            //it is more efficient to clear the serverMessage protobuf than creating a new one
            _serverMessage.Clear();

            //handle ok code based on its value
            switch (static_cast<client::OkCode>(okCode)) {
                case OkCode::authenticated:
                    Message::print(std::cout, "AUTH", "Authenticated");
                    return;

                //next ok codes should not be received here
                case OkCode::found:
                case OkCode::created:
                case OkCode::notThere:
                case OkCode::removed:
                case OkCode::retrieved:
                default:
                    throw ProtocolManagerException("Unexpected ok code",
                                                   ProtocolManagerError::unexpectedCode);
            }
        }
        case messages::ServerMessage_Type_ERR: {
            int errCode = _serverMessage.code();    //error code

            //it is more efficient to clear the serverMessage protobuf than creating a new one
            _serverMessage.Clear();

            //handle error code based on its value
            switch (static_cast<client::ErrCode>(errCode)) {
                case ErrCode::auth:
                    throw ProtocolManagerException("Authentication error",
                                                   ProtocolManagerError::auth);

                case ErrCode::exception:
                    throw ProtocolManagerException("Internal server error",
                                                   ProtocolManagerError::internal);

                //next error codes should not be received here
                case ErrCode::unexpected:
                case ErrCode::notAFile:
                case ErrCode::store:
                case ErrCode::remove:
                case ErrCode::notADir:
                case ErrCode::retrieve:
                default:
                    throw ProtocolManagerException("Unexpected error code",
                                                   ProtocolManagerError::unexpectedCode);
            }
        }
        case messages::ServerMessage_Type_VER:
            //server response contains the version to use
            Message::print(std::cout, "VER", "Change version to",
                           std::to_string(_serverMessage.newversion()));
            //for future use (not implemented now, to implement when a new version is written)
            //for now terminate the program

            //it is more efficient to clear the serverMessage protobuf than creating a new one
            _serverMessage.Clear();

            throw ProtocolManagerException("Version not supported", ProtocolManagerError::version);

        //next message type should not be received here
        case messages::ServerMessage_Type_NOOP:
        case messages::ServerMessage_Type_SEND:
        case messages::ServerMessage_Type_MKD:
        case messages::ServerMessage_Type_STOR:
        case messages::ServerMessage_Type_DATA:
        default:
            //it is more efficient to clear the serverMessage protobuf than creating a new one
            _serverMessage.Clear();

            Message::print(std::cerr, "ERROR", "Unexpected message type");

            throw ProtocolManagerException("Unexpected message type error",
                                           ProtocolManagerError::unexpected);
    }
}

/**
 * ProtocolManager canSend method.
 *  Used to know if the protocol manager can send a message, namely if the sent messages queue is not full
 *
 * @return true if there is space for another message to be sent, no otherwise
 *
 * @author Michele Crepaldi s269951
 */
bool client::ProtocolManager::canSend() const {
    return !_waitingForResponse.full();
}

/**
 * ProtocolManager isWaiting method.
 *  Used to know if the Protocol Manager is waiting for server responses or not, a.k.a. if the message queue is not
 *  empty.
 *
 * @return true if the Protocol Manager is waiting for server responses, false otherwise
 *
 * @author Michele Crepaldi s269551
 */
bool client::ProtocolManager::isWaiting() const{
    return !_waitingForResponse.empty();
}

/**
 * ProtocolManager nWaiting method.
 *  Used to get the number of messages waiting for a server response (in the message queue)
 *
 * @return number of messages waiting for a response
 *
 * @author Michele Crepaldi s269551
 */
int client::ProtocolManager::nWaiting() const{
    return _waitingForResponse.size();
}

/**
 * ProtocolManager recoverFromError method.
 *  Used to recover from errors or exceptions -> it re-sends all already sent messages for which we did not have a
 *  server response
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::recoverFromError() {
    int start = _waitingForResponse.start();    //waiting messages queue start index
    int end = _waitingForResponse.end();        //waiting messages queue end index
    int size = _waitingForResponse.size();      //waiting messages queue size

    //iterate over all messages in the waiting queue
    for(int i = start; i != end; i = (i+1)%size){
        Event event = _waitingForResponse[i];    //current element from the queue

        //if the event is of type storeSent
        if(event.getType() == FileSystemStatus::storeSent){
            std::string ap = event.getElement().getAbsolutePath();  //current element's absolute path

            //if the file to transfer is not present anymore in the filesystem or its hash is different from the one of
            //the file present in the filesystem then it means that the file was deleted or modified
            //--> I don't send it anymore
            if(!std::filesystem::exists(ap) ||
                    Directory_entry(_path_to_watch, ap).getHash() != event.getElement().getHash())
                break;
        }

        //compose message based on event
        _composeMessage(event);

        //again, if the event is of type storeSent
        if(event.getType() == FileSystemStatus::storeSent){
            _sendFile(event.getElement()); //send file
        }
    }
}

/**
 * ProtocolManager send method.
 *  It is used to send an event message to the server
 *
 * @param event event to create the message from
 *
 * @return true if event was inserted in the waiting for response queue and it was sent (or if it was of an unsupported
 * type); false otherwise
 *
 * @author Michele Crepaldi s269551
 */
bool client::ProtocolManager::send(Event &event) {

    if(_waitingForResponse.full())
        return false;

    //if the element is not a file nor a directory then return (it is not of a supported type)
    if (!event.getElement().is_regular_file() && !event.getElement().is_directory()) {
        Message::print(std::cerr, "WARNING", "Change to an unsupported type",
                       event.getElement().getRelativePath());
        return true;
    }

    //compose the message based on the event (and send it)
    _composeMessage(event);

    //save a copy of the event in the message waiting queue
    _waitingForResponse.push(event);

    return true;
}

/**
 * ProtocolManager receive method.
 *  Used to receive and process messages from the server
 *
 * @throws ProtocolManagerException:
 *  <b>version</b> if the server message uses a different version or if it asked for a change of version
 * @throws ProtocolManagerException:
 *  <b>protocol</b> if there are errors in the serverMessage
 * @throws ProtocolManagerException:
 *  <b>unexpectedCode</b> if an unexpected ok/error code was found inside the serverMessage
 * @throws ProtocolManagerException:
 *  <b>client</b> if the server reported an error in the client message sent or if server encountered an unexpected
 *  client message
 * @throws ProtocolManagerException:
 *  <b>internal</b> if the server reported a server internal error
 * @throws ProtocolManagerException:
 *  <b>unexpected</b> if an unexpected server message was received
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::receive() {
    //get server response

    std::string server_temp = _s.recvString();      //server response message
    _serverMessage.ParseFromString(server_temp);    //get serverMessage protobuf parsing the response message

    //get the first event on the waiting list -> this is the message we received a response for

    Event event = _waitingForResponse.front();  //current event (the head of the waiting queue)

    //check serverMessage version
    if(_protocolVersion != _serverMessage.version()) {
        //it is more efficient to clear the serverMessage protobuf than creating a new one
        _serverMessage.Clear();

        throw ProtocolManagerException("Server is using a different version",
                                       client::ProtocolManagerError::version);
    }

    //switch on server message type
    switch (_serverMessage.type()) {
        case messages::ServerMessage_Type_SEND:
            //check that the event's element is actually a file and that the event type is "created" or "modified"
            //(otherwise I don't have to send it)
            if(event.getElement().is_regular_file() && (event.getType() == FileSystemStatus::created ||
                    event.getType() == FileSystemStatus::modified)){

                //send file

                std::string path = _serverMessage.path();   //path got from serverMessage
                Hash h = Hash(_serverMessage.hash());   //hash got from serverMessage

                //it is more efficient to clear the serverMessage protobuf than creating a new one
                _serverMessage.Clear();

                //check that actually the send message refers to the current event
                if(event.getElement().getRelativePath() != path || event.getElement().getHash() != h)
                    throw ProtocolManagerException("Error in the server message",
                                                   ProtocolManagerError::serverMessage);

                //remove message (PROB) event from queue (it was successful)
                _waitingForResponse.pop();

                //if the file to transfer is not present anymore in the filesystem or its hash is different from the one of
                //the file present in the filesystem then it means that the file was deleted or modified
                //--> I don't send it anymore
                if(!std::filesystem::exists(event.getElement().getAbsolutePath()) ||
                   Directory_entry(_path_to_watch, event.getElement().getAbsolutePath()).getHash()
                   != event.getElement().getHash())
                    break;

                //(file created/modified) -> the store message was sent to server event
                Event newEvent = Event(event.getElement(), FileSystemStatus::storeSent);

                //compose the message based on the event (and send it)
                _composeMessage(newEvent);

                //save a copy of the event in the message waiting queue
                _waitingForResponse.push(std::move(newEvent));

                //send the file
                _sendFile(event.getElement());
                break;
            }
            //if I am here then I got a send message but the element is not a file so this is a error
            Message::print(std::cerr, "ERROR", "protocol error");

            throw ProtocolManagerException("Error in the server message",
                                           ProtocolManagerError::serverMessage);

        case messages::ServerMessage_Type_OK: {
            //last command was successful

            int okCode = _serverMessage.code(); //ok code got from serverMessage

            //it is more efficient to clear the serverMessage protobuf than creating a new one
            _serverMessage.Clear();

            //handle ok code based on its value
            switch (static_cast<client::OkCode>(okCode)) {
                case OkCode::found:
                    Message::print(std::cout, "SUCCESS", "PROB", event.getElement().getRelativePath());
                    break;

                case OkCode::created:
                    Message::print(std::cout, "SUCCESS", "STOR/MKD", event.getElement().getRelativePath());
                    break;

                case OkCode::notThere:
                case OkCode::removed:
                    Message::print(std::cout, "SUCCESS", "DELE/RMD", event.getElement().getRelativePath());
                    break;

                //next codes are not expected
                case OkCode::authenticated:
                case OkCode::retrieved:
                default:
                    throw ProtocolManagerException("Unexpected ok code",
                                                   ProtocolManagerError::unexpectedCode);

            }

            //if the event which was successful was of type created, modified or storeSent
            if (event.getType() == FileSystemStatus::created || event.getType() == FileSystemStatus::modified ||
                event.getType() == FileSystemStatus::storeSent) {

                //insert or update element in db
                if (event.getType() == FileSystemStatus::created || event.getType() == FileSystemStatus::storeSent)
                    _db->insert(event.getElement()); //insert element into db
                else
                    _db->update(event.getElement()); //update element in db
            }   //otherwise
            else if (event.getType() == FileSystemStatus::deleted)
                _db->remove(event.getElement().getRelativePath()); //delete element from db

            //remove message event from queue (it was successful)
            _waitingForResponse.pop();

            break;
        }

        case messages::ServerMessage_Type_ERR: {
            //last command generated an error

            int errCode = _serverMessage.code();    //error code got from serverMessage

            //it is more efficient to clear the serverMessage protobuf than creating a new one
            _serverMessage.Clear();

            //handle error code based on its value
            switch (static_cast<client::ErrCode>(errCode)) {
                case ErrCode::notAFile:
                case ErrCode::store:
                case ErrCode::remove:
                case ErrCode::notADir:
                case ErrCode::unexpected:
                    //skip the event message that caused the error and go on
                    _waitingForResponse.pop();

                    Message::print(std::cerr, "WARNING", "Server reported an error in one sent message",
                                   "It will be skipped");
                    break;

                case ErrCode::exception:
                    throw ProtocolManagerException("Internal server error",
                                                   ProtocolManagerError::internal);

                case ErrCode::auth:
                case ErrCode::retrieve:
                default:
                    throw ProtocolManagerException("Unexpected error code",
                                                   ProtocolManagerError::unexpectedCode);
            }
            break;
        }

        case messages::ServerMessage_Type_VER:
            throw ProtocolManagerException("Version not supported", ProtocolManagerError::version);

        case messages::ServerMessage_Type_NOOP:
        case messages::ServerMessage_Type_MKD:
        case messages::ServerMessage_Type_STOR:
        case messages::ServerMessage_Type_DATA:
        default:
            Message::print(std::cerr, "ERROR", "Unexpected message type", "");
            throw ProtocolManagerException("Unexpected server message type",
                                           client::ProtocolManagerError::unexpected);
    }
}

/**
 * ProtocolManager retrieveFiles method.
 *  Used to ask the server to send all the user's requested files to this client
 *
 * @param macAddress optional user's mac address to retrieve data about
 * @param all boolean indicating if to retrieve all user's data or just data related to the optionally provided mac
 * @param destFolder destination folder where to put the user's files
 *
 * @throws ProtocolManagerException:
 *  <b>version</b> if the server is using a different protocol version
 * @throws ProtocolManagerException:
 *  <b>unexpectedCode</b> if an unexpected ok/error code was found in the server message
 * @throws ProtocolManagerException:
 *  <b>client</b> if the server reported an error in a client message
 * @throws ProtocolManagerException:
 *  <b>internal</b> if the server reported a server internal error
 * @throws ProtocolManagerException:
 *  <b>unexpected</b> if an unexpected server message type was received
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::retrieveFiles(const std::string &macAddress, bool all, const std::string &destFolder){
    //send RETR message to the server
    _send_RETR(macAddress, all);

    std::string tempDir = destFolder  + TEMP_RELATIVE_PATH; //temporary folder path (where to put temporary files)

    //loop until server indicates the end of the data transfer
    while(true) {
        //get server response

        std::string server_temp = _s.recvString();      //server response message
        _serverMessage.ParseFromString(server_temp);    //get serverMessage protobuf parsing the response message

        //check server message version
        if (_protocolVersion != _serverMessage.version())
            throw ProtocolManagerException("Server is using a different version",
                                           client::ProtocolManagerError::version);

        //switch on server message type
        switch (_serverMessage.type()) {
            case messages::ServerMessage_Type_MKD:
                _makeDir(destFolder);   //create directory
                break;

            case messages::ServerMessage_Type_STOR:
                _storeFile(destFolder, tempDir);    //store file sent from server
                break;

            case messages::ServerMessage_Type_OK: {
                //end of the server data transfer -> return

                int okCode = _serverMessage.code(); //ok code got from server message

                //it is more efficient to clear the serverMessage protobuf than creating a new one
                _serverMessage.Clear();

                //handle ok code based on its value
                switch (static_cast<client::OkCode>(okCode)) {
                    case OkCode::retrieved:
                        Message::print(std::cout, "SUCCESS", "RETR",
                                       "Saved all your data to " + destFolder);
                        return;

                    case OkCode::found:
                    case OkCode::created:
                    case OkCode::notThere:
                    case OkCode::removed:
                    case OkCode::authenticated:
                    default:
                        throw ProtocolManagerException("Unexpected OK code",
                                                       ProtocolManagerError::unexpectedCode);

                }
            }

            case messages::ServerMessage_Type_ERR: {

                int errCode = _serverMessage.code();    //ok code got from server message

                //it is more efficient to clear the serverMessage protobuf than creating a new one
                _serverMessage.Clear();

                //handle error code based on its value
                switch (static_cast<client::ErrCode>(errCode)) {
                    case ErrCode::retrieve:
                        throw ProtocolManagerException("Client error",
                                                       ProtocolManagerError::client);

                    case ErrCode::exception:
                        throw ProtocolManagerException("Internal server error",
                                                       ProtocolManagerError::internal);

                    case ErrCode::auth:
                    case ErrCode::notAFile:
                    case ErrCode::store:
                    case ErrCode::remove:
                    case ErrCode::notADir:
                    case ErrCode::unexpected:
                    default:
                        throw ProtocolManagerException("Unexpected error code",
                                                       ProtocolManagerError::unexpectedCode);
                }
            }

            case messages::ServerMessage_Type_VER:
                throw ProtocolManagerException("Version not supported", ProtocolManagerError::version);

            case messages::ServerMessage_Type_NOOP:
            case messages::ServerMessage_Type_SEND:
            case messages::ServerMessage_Type_DATA:
            default: //unexpected message type
                Message::print(std::cerr, "ERROR", "Unexpected message type", "");
                throw ProtocolManagerException("Unexpected server message type",
                                               client::ProtocolManagerError::unexpected);
        }
    }
}

/**
 * ProtocolManager send clientMessage method.
 *  It will send the clientMessage and then clear it
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_clientMessage(){
    //string representation of the clientMessage protobuf
    std::string tmp = _clientMessage.SerializeAsString();

    //send response message
    _s.sendString(tmp);

    //it is more efficient to clear the clientMessage protobuf than creating a new one
    _clientMessage.Clear();
}

/**
 * ProtocolManager send AUTH message method.
 *  It will set the clientMessage protobuf version, type, username, mac address and password and then send it
 *
 * @param username username of the user who wants to authenticate
 * @param macAddress mac address of the user's machine
 * @param password password of the user who wants to authenticate
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_AUTH(const std::string &username, const std::string &macAddress, const std::string &password){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_AUTH);

    //set username, macAddress and password
    _clientMessage.set_username(username);
    _clientMessage.set_macaddress(macAddress);
    _clientMessage.set_password(password);

    _send_clientMessage();
}

/**
 * ProtocolManager send PROB message method.
 *  It will set the clientMessage protobuf version, type, path, last write time and hash and then send it
 *
 * @param element Directory_entry element (file) to PROB on server
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_PROB(Directory_entry &element){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_PROB);

    //set the path, last write time and hash
    _clientMessage.set_path(element.getRelativePath());
    _clientMessage.set_lastwritetime(element.getLastWriteTime());
    _clientMessage.set_hash(element.getHash().get().first, element.getHash().get().second);

    _send_clientMessage();
}

/**
 * ProtocolManager send DELE message method.
 *  It will set the clientMessage protobuf version, type, path and hash and then send it
 *
 * @param element Directory_entry element (file) to delete on server
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_DELE(Directory_entry &element){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_DELE);

    //set path and hash
    _clientMessage.set_path(element.getRelativePath());
    _clientMessage.set_hash(element.getHash().get().first, element.getHash().get().second);

    _send_clientMessage();
}

/**
 * ProtocolManager send STOR message method.
 *  It will set the clientMessage protobuf version, type, path, file size, last write time and hash and then send it
 *
 * @param element Directory_entry element (file) to store on server
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_STOR(Directory_entry &element){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_STOR);

    //set path, file size, last write time and hash
    _clientMessage.set_path(element.getRelativePath());
    _clientMessage.set_filesize(element.getSize());
    _clientMessage.set_lastwritetime(element.getLastWriteTime());
    _clientMessage.set_hash(element.getHash().get().first, element.getHash().get().second);

    _send_clientMessage();
}

/**
 * ProtocolManager send DATA message method.
 *  It will set the clientMessage protobuf version, type, data and then send it
 *
 * @param buff buffer with data to send to server
 * @param len length of the data in buff to send to server
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_DATA(char *buff, uint64_t len){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_DATA);

    //set data (with buff)
    _clientMessage.set_data(buff, len);

    _send_clientMessage();
}

/**
 * ProtocolManager send MKD message method.
 *  It will set the clientMessage protobuf version, type, path and last write time and then send it
 *
 * @param element Directory_entry element (directory) to store on server
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_MKD(Directory_entry &element){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_MKD);

    //set path and last write time
    _clientMessage.set_path(element.getRelativePath());
    _clientMessage.set_lastwritetime(element.getLastWriteTime());

    _send_clientMessage();
}

/**
 * ProtocolManager send RMD message method.
 *  It will set the clientMessage protobuf version, type, path and then send it
 *
 * @param element Directory_entry element (directory) to remove on server
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_RMD(Directory_entry &element){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_RMD);

    //set path
    _clientMessage.set_path(element.getRelativePath());

    _send_clientMessage();
}

/**
 * ProtocolManager composeMessage method.
 *  Used to compose a clientMessage from an event
 *
 * @param event event to transform in message (and send)
 *
 * @throws ProtocolManagerException:
 *  <b>client</b> if a filesystem status is not supported
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_composeMessage(Event &event) {

    if (event.getElement().is_regular_file()) { //if the element is a file
        //switch on the event type
        switch (event.getType()) {
            case FileSystemStatus::modified:
            case FileSystemStatus::created:
                Message::print(std::cout, "EVENT", "File created/modified",
                               event.getElement().getRelativePath());

                _send_PROB(event.getElement()); //send PROB message
                break;

            case FileSystemStatus::deleted:
                Message::print(std::cout, "EVENT", "File deleted",
                               event.getElement().getRelativePath());

                _send_DELE(event.getElement()); //send DELE message
                break;

            case FileSystemStatus::storeSent:

                _send_STOR(event.getElement()); //send STOR + DATA(+) messages
                break;

            default:    //I should never arrive here
                Message::print(std::cerr, "WARNING", "Filesystem status not supported");
                throw ProtocolManagerException("Filesystem status not supported",
                                               ProtocolManagerError::client);
        }
    } else { //if the element is a directory
        //switch on the event type
        switch (event.getType()) {
            case FileSystemStatus::modified:
            case FileSystemStatus::created:
                Message::print(std::cout, "EVENT", "Directory created/modified",
                               event.getElement().getRelativePath());

                _send_MKD(event.getElement());
                break;

            case FileSystemStatus::deleted:
                Message::print(std::cout, "EVENT", "Directory deleted",
                               event.getElement().getRelativePath());

                _send_RMD(event.getElement());
                break;

            case FileSystemStatus::storeSent:
            default:    //I should never arrive here
                Message::print(std::cerr, "WARNING", "Filesystem status not supported");
                throw ProtocolManagerException("Filesystem status not supported",
                                               ProtocolManagerError::client);
        }
    }
}

/**
 * ProtocolManager sendFile method.
 *  Used to send a file to the server through messages
 *
 * @param element Directory_entry representing the file to send
 *
 * @throws ProtocolManagerException:
 *  <b>client</b> if the file is not present any more in the system or if the file could not be opened
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_sendFile(Directory_entry &element) {
    std::ifstream file;             //file to send
    char buff[_maxDataChunkSize];   //buffer used to read from file and send to socket

    //the file should exist and be the same as expected since I checked these things before calling _sendFile

    //open input file
    file.open(element.getAbsolutePath(), std::ios::in | std::ios::binary);

    if(file.is_open()){
        int64_t totRead = 0;    //total bytes read

        Message message{"SENDING", "Sending file:", element.getRelativePath()};
        std::cout << message;

        while(file.read(buff, _maxDataChunkSize)) { //read file in max_data_chunk_size-wide blocks
            totRead += file.gcount();   //update total bytes read
            _send_DATA(buff, file.gcount());    //send the data block

            //update the progress bar in the message
            message.update(std::floor((float)100.0 * totRead/element.getSize()));
            std::cout << message;
        }

        _clientMessage.set_last(true);   //mark the last data block

        totRead += file.gcount();   //update total bytes read
        _send_DATA(buff, file.gcount()); //send the last data block

        //update the progress bar in the message
        if(totRead != 0)
        {
            message.update(std::floor((float)100.0 * totRead / element.getSize()));
        }
        else
            message.update(100);

        std::cout << message << std::endl;

        //close the input file
        file.close();
    }
    else
        throw ProtocolManagerException("Could not open file", ProtocolManagerError::client);
}

/**
 * ProtocolManager send RETR message method.
 *  It will set the clientMessage protobuf version, type, mac address and all boolean and then send it
 *
 * @param macAddress mac address to retrieve the data about from server
 * @param all if to retrieve all the data about the user or only the ones related to the user-mac pair
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_send_RETR(const std::string &macAddress, bool all){
    _clientMessage.set_version(_protocolVersion);
    _clientMessage.set_type(messages::ClientMessage_Type_RETR);

    //set mac address and all boolean
    _clientMessage.set_macaddress(macAddress);
    _clientMessage.set_all(all);

    _send_clientMessage();
}

/**
 * ProtocolManager storeFile method.
 *  Used to interpret the STOR message sent by server and to get all the DATA messages for a file;
 *  it stores the file in a temporary directory with a temporary random name; when the
 *  file transfer is done then it checks the file was correctly saved and moves it to
 *  the final destination (overwriting any old existing file).
 *
 * @param destFolder destination folder where to put files
 * @param temporaryPath temporary path where to put temporary files
 *
 * @throws ProtocolManagerException:
 *  <b>version</b> if the server is using a different protocol version
 * @throws ProtocolManagerException:
 *  <b>internal</b> if the server had an error and the file transfer was interrupted by another message type
 * @throws ProtocolManagerException:
 *  <b>internal</b> if the stored file has a different dimension than expected
 * @throws ProtocolManagerException:
 *  <b>internal</b> if errors were found in the server message (validation failed)
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_storeFile(const std::string &destFolder, const std::string &temporaryPath){

    std::string path = _serverMessage.path();                   //file relative path
    uintmax_t size = _serverMessage.filesize();                 //file size
    std::string lastWriteTime = _serverMessage.lastwritetime(); //file last write time
    Hash h = Hash(_serverMessage.hash());                       //file hash

    //it is more efficient to clear the serverMessage protobuf than creating a new one
    _serverMessage.Clear();


    //validate path got from serverMessage
    if(!Validator::validatePath(path))
        throw ProtocolManagerException("Path validation failed",
                                       ProtocolManagerError::serverMessage);

    //validate last write time got from serverMessage
    if(!Validator::validateLastWriteTime(lastWriteTime))
        throw ProtocolManagerException("Last write time validation failed",
                                       ProtocolManagerError::serverMessage);


    //expected Directory entry element (got from the server message)
    Directory_entry expected{destFolder, path, size, "file", lastWriteTime, h};

    Message::print(std::cout, "STOR", expected.getRelativePath(), "in " + destFolder);

    //create a random temporary name for the file

    RandomNumberGenerator rng;  //random number generator
    std::string tmpFileName = "/" + rng.getHexString(_tempNameSize) + ".tmp";   //temporary file name

    //check if the temporary folder already exists
    bool tmpPathExists = std::filesystem::exists(temporaryPath);

    //if the temporary directory does not already exist
    if(!tmpPathExists)
        //create all the directories (that do not already exist) up to the temporary path
        std::filesystem::create_directories(temporaryPath);

    std::ofstream temporaryFile; //temporary file

    //create the temporary file
    temporaryFile.open(temporaryPath + tmpFileName, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if(temporaryFile.is_open()){
        try {
            bool loop = true;
            int64_t totRecv = 0;    //total number of bytes received from server

            Message msg{"RECV", "Receiving file:", expected.getRelativePath()};
            std::cout << msg;

            while(loop){
                //receive message from server

                std::string message = _s.recvString();      //message got from client

                //convert message to clientMessage protobuf
                _serverMessage.ParseFromString(message);

                //check message version
                if (_serverMessage.version() != _protocolVersion) { //if the version is different
                    //it is more efficient to clear the clientMessage protobuf than creating a new one
                    _serverMessage.Clear();

                    //close the temporary file
                    temporaryFile.close();

                    //delete the temporary file
                    std::filesystem::remove(temporaryPath + tmpFileName);

                    //throw exception
                    throw ProtocolManagerException("Server is using a different version",
                                                   ProtocolManagerError::version);
                }

                //check message type, it has to be DATA type
                if (_serverMessage.type() != messages::ServerMessage_Type_DATA) {   //if it is not of type DATA
                    //it is more efficient to clear the clientMessage protobuf than creating a new one
                    _serverMessage.Clear();

                    //close the temporary file
                    temporaryFile.close();

                    //delete the temporary file
                    std::filesystem::remove(temporaryPath + tmpFileName);

                    //no DATA message with "last" boolean set was encountered before this message, error!

                    throw ProtocolManagerException("Unexpected message, DATA transfer was not done.",
                                                   ProtocolManagerError::serverMessage);
                }

                //is this the last data packet? if yes stop the loop
                loop = !_serverMessage.last();

                //data got from the clientMessage protobuf
                std::string data = _serverMessage.data();

                //write the data to temporary file
                temporaryFile.write(data.data(), data.size());

                totRecv += data.size(); //update total bytes received

                //update the progress bar in the message
                if(totRecv != 0)
                    msg.update(std::floor((float)100.0 * totRecv/expected.getSize()));
                else
                    msg.update(100);

                std::cout << msg;

                //it is more efficient to clear the clientMessage protobuf than creating a new one
                _serverMessage.Clear();
            }

            std::cout << std::endl;
        }
        //in case of socket exceptions while transferring the file I need to delete the temporary file
        catch (SocketException &e) {
            //close the temporary file
            temporaryFile.close();

            //delete the temporary file
            std::filesystem::remove(temporaryPath + tmpFileName);

            //re-throw the exception
            throw;
        }

        //close the temporary file
        temporaryFile.close();

        //Directory entry which represents the newly created file
        Directory_entry newFile{temporaryPath, std::filesystem::directory_entry(temporaryPath + tmpFileName)};

        //change last write time for the temporary file to what was expected
        newFile.set_time_to_file(expected.getLastWriteTime());

        //temporary file hash
        Hash hash = newFile.getHash();

        //check if the newly created (temporary) file properties match the expected ones
        if(newFile.getSize() != expected.getSize() || hash != expected.getHash() ||
                newFile.getLastWriteTime() != expected.getLastWriteTime()) {

            //if the temporary file is not as we expected

            //delete the temporary file
            std::filesystem::remove(temporaryPath + tmpFileName);

            throw ProtocolManagerException("Stored file is different than expected.",
                                           ProtocolManagerError::serverMessage);
        }

        //get the file parent path from the expected file name

        //file parent path
        std::filesystem::path parentPath = std::filesystem::path(expected.getAbsolutePath()).parent_path();

        //check if the parent folder already exists
        bool parentExists = std::filesystem::exists(parentPath);

        //if the parent directory does not exist
        if(!parentExists)
            //create all the directories (that do not already exist) up to the parent path
            std::filesystem::create_directories(parentPath);

        //save the lastWriteTime of the destination folder (parent directory) before moving the file,
        //any lastWriteTime modification to that directory will be requested explicitly by the server,
        //so we want to keep the same time before and after the file move

        //Directory entry representing the file parent directory
        Directory_entry parent;

        //only if the parent path is different from client destFolder get parent Directory entry (otherwise we
        //have problems getting relative path
        if(parentPath.string() != destFolder)
            parent = {destFolder, parentPath.string()};

        //If we are here then the file was successfully transferred and its copy on the server is as expected
        //it can be moved to the final destination
        std::filesystem::rename(temporaryPath + tmpFileName, expected.getAbsolutePath());

        //if the parent directory is not the server base path
        if(parent.getAbsolutePath() != destFolder)
            //reset the parent directory lastWriteTime
            parent.set_time_to_file(parent.getLastWriteTime());

        return;
    }

    //some error occurred
    throw ProtocolManagerException("Could not open file", ProtocolManagerError::client);
}

/**
 * ProtocolManager makeDir method.
 *  Used to create/(modify its last write time) a directory on the client filesystem
 *
 * @param destFolder destination folder where to create the directory
 *
 * @throws ProtocolManagerException:
 *  <b>internal</b> if the server tried to modify something which is not a directory
 * @throws ProtocolManagerException:
 *  <b>internal</b> if there were errors in the server message (validation failed)
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::_makeDir(const std::string &destFolder){
    std::string path = _serverMessage.path();                     //dir relative path
    std::string lastWriteTime = _serverMessage.lastwritetime();   //dir last write time

    //it is more efficient to clear the serverMessage protobuf than creating a new one
    _serverMessage.Clear();


    //validate path got from clientMessage
    if(!Validator::validatePath(path))
        throw ProtocolManagerException("Path validation failed",
                                       ProtocolManagerError::serverMessage);

    //validate last write time got from clientMessage
    if(!Validator::validateLastWriteTime(lastWriteTime))
        throw ProtocolManagerException("Last write time validation failed",
                                       ProtocolManagerError::serverMessage);


    Message::print(std::cout, "MKD", path, "in " + destFolder);

    //get the dir parent path

    //dir parent path
    auto parentPath = std::filesystem::path(destFolder + path).parent_path();

    //check if the parent folder already exists
    bool parentExists = std::filesystem::exists(parentPath.string());

    //Directory entry representing the dir parent folder
    Directory_entry parent;

    //if the parent directory exists and the parent path is different from server base path
    if(parentExists && parentPath.string() != destFolder)
        //save the lastWriteTime of the destination folder (parent directory) before creating the directory,
        //any lastWriteTime modification to that directory will be requested explicitly by the client,
        //so we want to keep the same time before and after the directory create
        parent = Directory_entry{destFolder, parentPath.string()};

    //if the directory does not already exist
    if(!std::filesystem::exists(destFolder + path))
        //create all the directories (that do not already exist) up to the path
        std::filesystem::create_directories(destFolder + path);

    //in case the it existed, if it is not a directory
    if(!std::filesystem::is_directory(destFolder + path)){
        //the element is not a directory! (in case of a modify) -> error

        throw ProtocolManagerException("Tried to modify something which is not a directory.",
                                       ProtocolManagerError::serverMessage);
    }

    //Directory entry which represents the newly created folder (or the already present one)
    Directory_entry newDir{destFolder, std::filesystem::directory_entry(destFolder + path)};

    //change last write time for the directory
    newDir.set_time_to_file(lastWriteTime);

    //if the parent directory already existed before (and it is not the base path)
    if(parentExists && parentPath.string() != destFolder)
        //reset the parent directory lastWriteTime
        parent.set_time_to_file(parent.getLastWriteTime());
}