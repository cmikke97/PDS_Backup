//
// Created by michele on 26/09/2020.
//

#include <fstream>
#include <utility>
#include "ProtocolManager.h"
#include "../server/ProtocolManager.h"


/**
 * constructor of protocol manager class
 *
 * @param s socket to read/write to/from
 * @param db Database object to save/modify/delete entries to/from
 * @param max max number of messages sent without response
 * @param ver verion of the protocol
 * @param maxTries maximum number of successive re-tries the client can do for the same message (which has received an error response)
 *
 * @author Michele Crepaldi s269551
 */
client::ProtocolManager::ProtocolManager(Socket &s, int max, int ver, int maxTries, std::string  path) : s(s), start(0), end(0), tries(0), maxTries(maxTries), size(max), protocolVersion(ver), path_to_watch(std::move(path)) {
    auto config = Config::getInstance(CONFIG_FILE_PATH);
    waitingForResponse.resize(size+1);
    db = Database::getInstance(config->getDatabasePath());
}

/**
 * function used to authenticate a user to the service
 *
 * @param username username of the user
 * @param password password of the user
 * @param macAddress macAddress of this machine
 *
 * @throw exception ProtocolManagerException("Authentication error", code) in case of an authentication error (wrong username and password)
 * @throw exception ProtocolManagerException("Internal server error", code, tries) in case of a server error -> try again connection
 * @throw exception ProtocolManagerException("Unknown server error", 0, 0) in case of an unknown server error
 * @throw (for now) exception ProtocolManagerException("Version not supported", 505, serverMessage.newversion()) in case of a change version request
 * @throw exxcpetion ProtocolManagerException("Unsupported message type error", -1, 0) in case of an unsupported message type
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::authenticate(const std::string& username, const std::string& password, const std::string& macAddress) {

    //send authentication message to server
    send_AUTH(username, macAddress, password);

    //get server response
    std::string server_temp = s.recvString();

    //parse server response
    serverMessage.ParseFromString(server_temp);
    //std::cout << serverMessage.type();

    int code;
    switch (serverMessage.type()) {
        case messages::ServerMessage_Type_OK:
            //authentication ok
            code = serverMessage.code();
            switch(static_cast<client::okCode>(code)) {
                case okCode::authenticated: //user authentication successful
                    std::cout << "authenticated, code " << code << std::endl;
                    return;
                case okCode::found: //probe file found the file
                case okCode::created:   //file/directory create was successful
                case okCode::notThere:  //file/directory remove did not find the file/directory (the end effect is the same as a successful remove)
                case okCode::removed:   //file/directory remove was successful
                default:
                    throw ProtocolManagerException("Unexpected code", protocolManagerError::unexpected);

            }
        case messages::ServerMessage_Type_ERR:
            //retrieve error code
            code = serverMessage.code();

            //based on error code handle it
            switch(static_cast<client::errCode>(code)){
                case errCode::auth: //authentication error
                    throw ProtocolManagerException("Authentication error", protocolManagerError::auth);
                case errCode::exception:    //there was an exception in the server
                    throw ProtocolManagerException("Internal server error", protocolManagerError::internal);
                case errCode::unexpected:   //unexpected message type
                case errCode::notAFile: //error in PROB -> the element is not a file
                case errCode::store:    //error in STOR -> the written file is different from what expected (different size, hash or lastWriteTime)
                case errCode::remove:   //error in DELE -> the element is not a file or the hash does not correspond
                case errCode::notADir:  //error in RMD/MKD -> the element is not a directory
                    throw ProtocolManagerException("Unexpected error code", protocolManagerError::unexpected);
                default:    //unknown server error
                    throw ProtocolManagerException("Unknown server error", protocolManagerError::unknown);
            }
        case messages::ServerMessage_Type_VER:
            //server response contains the version to use
            std::cout << "Change version to: " << serverMessage.newversion() << std::endl;
            //for future use (not implemented now, to implement when a new version is written)
            //for now terminate the program
            throw ProtocolManagerException("Version not supported", protocolManagerError::version);

        default: //i should never arrive here
            std::cerr << "Unsupported message type" << std::endl;
            throw ProtocolManagerException("Unsupported message type error", protocolManagerError::unsupported);
    }

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    //serverMessage.Clear();
}

/**
 * function to quit from service (connection)
 *
 * @author Michele Crepaldi s269551
 */
 /*
void client::ProtocolManager::quit() {
    //create message
    send_QUIT();

    //TODO decide if to get a message back or not
}
*/

/**
 * function used to send a message relative to an event to the server
 *
 * @param e event to create the message from
 *
 * @throw SocketException in case it cannot write data to socket
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send(Event &e) {
    if (!e.getElement().is_regular_file() && !e.getElement().is_directory()) {  //if it is not a file nor a directory then return
        std::cerr << "change to an unsupported type." << std::endl;
        return;
    }

    //compose the message based on event
    composeMessage(e);

    //save a copy of the event in a local list (to then wait for its response)
    waitingForResponse[end] = e;
    end = (end+1)%size;
}

/**
 * function used to receive and process messages from server
 *
 * @throw SocketException in case it cannot read data from socket
 * @throw ProtocolManagerException("Internal server error", 500, 1) in case of a repeated (more than X times) error for the same sent message
 * @throw ProtocolManagerException("Unknown server error", 0, 0) in case of an unknown server error
 * @throw ProtocolManagerException("Unsupported message type error", -1, 0) in case of an unsupported message type error
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::receive() {
    //get server response
    std::string server_temp = s.recvString();

    //parse server response
    serverMessage.ParseFromString(server_temp);

    //get the first event on the waiting list -> this is the message we received a response for
    Event e = waitingForResponse[start];

    //check version
    if(protocolVersion != serverMessage.version())
        throw ProtocolManagerException("Different protocol version!", client::protocolManagerError::version); //TODO decide if to keep this

    int code;
    switch (serverMessage.type()) {
        case messages::ServerMessage_Type_SEND:
            //only if the element is a file then it is right to send it
            if(e.getElement().is_regular_file() && (e.getStatus() == FileSystemStatus::created || e.getStatus() == FileSystemStatus::modified)){
                //send file

                /* TODO evaluate if to add this
                //check if the SEND MESSAGE actually refers to the first waiting event; if not then there was an error
                if(serverMessage.path() != e.getElement().getRelativePath() || serverMessage.filesize() != e.getElement().getSize() ||
                   serverMessage.lastwritetime() != e.getElement().getLastWriteTime() || Hash(serverMessage.hash()) != e.getElement().getHash())
                    throw ProtocolManagerException("Response SEND message does not match the current event", client::protocolManagerError::unknown, 0);
                */

                //if(start != end), this condition is true if I receive something
                //pop the (create) event element (it was successful)
                start = (start+1)%size;
                //reset the tries variable (i popped a message)
                tries = 0;

                //if the file to transfer is not present anymore in the filesystem or its hash is different from the one of the file present in the filesystem
                //then it means that the file was deleted or modified --> I can't send it anymore
                std::filesystem::path ap = e.getElement().getAbsolutePath();
                if(!std::filesystem::directory_entry(ap).exists() || Directory_entry(path_to_watch, ap).getHash() != e.getElement().getHash())
                    break;

                //otherwise send file

                //distinguish between file creation and modification in the waiting queue (in order to know what to do later)
                //TODO reconsider this
                Event tmp;
                if(e.getStatus() == FileSystemStatus::created)
                    tmp = Event(e.getElement(), FileSystemStatus::storeSent); //file created
                else
                    tmp = Event(e.getElement(), FileSystemStatus::modifySent); //file modified

                //compose the message based on send event
                composeMessage(tmp);

                //save the send event in a local list (to then wait for its response)
                waitingForResponse[end] = std::move(tmp);
                end = (end+1)%size;

                sendFile(e.getElement().getAbsolutePath());
                break;
            }
            //if I am here then I got a send message but the element is not a file so this is a protocol error (I should never get here)
            //TODO throw exception
            std::cerr << "protocol error" << std::endl;
            break;
        case messages::ServerMessage_Type_OK:
            //if(start != end) this condition is true if I receive something
            //pop the event element (it was successful)
            start = (start+1)%size;
            //reset the tries variable (i popped a message)
            tries = 0;

            std::cout << "Command for " << e.getElement().getAbsolutePath() << " has been execute in server: " << serverMessage.code() << std::endl;

            code = serverMessage.code();

            switch(static_cast<client::okCode>(code)) {
                case okCode::found:     //probe file found the file
                    std::cout << "Command successful: PROB of " << e.getElement().getRelativePath() << std::endl;
                    break;

                case okCode::created:   //file/directory create was successful
                    std::cout << "Command successful: STOR/MKD of " << e.getElement().getRelativePath() << std::endl;
                    break;

                case okCode::notThere:  //file/directory remove did not find the file/directory (the end effect is the same as a successful remove)
                case okCode::removed:   //file/directory remove was successful
                    std::cout << "Command successful: DELE/RMD of " << e.getElement().getRelativePath() << std::endl;
                    break;

                case okCode::authenticated: //user authentication successful
                default:
                    throw ProtocolManagerException("Unexpected code", protocolManagerError::unexpected);

            }

            if(e.getStatus() == FileSystemStatus::created || e.getStatus() == FileSystemStatus::modified ||
                e.getStatus() == FileSystemStatus::storeSent || e.getStatus() == FileSystemStatus::modifySent){

                //insert or update element in db
                if(e.getStatus() == FileSystemStatus::created || e.getStatus() == FileSystemStatus::storeSent)
                    db->insert(e.getElement()); //insert element into db
                else
                    db->update(e.getElement()); //update element in db
            }
            else if(e.getStatus() == FileSystemStatus::deleted)
                db->remove(e.getElement().getRelativePath()); //delete element from db

            break;

        case messages::ServerMessage_Type_ERR:
            //retrieve error code
            code = serverMessage.code();
            std::cerr << "Error: code " << code << std::endl;

            //based on error code handle it
            switch(static_cast<client::errCode>(code)){
                case errCode::notAFile: //error in PROB -> the element is not a file
                case errCode::store:    //error in STOR -> the written file is different from what expected (different size, hash or lastWriteTime)
                case errCode::remove:   //error in DELE -> the element is not a file or the hash does not correspond
                case errCode::notADir:  //error in RMD/MKD -> the element is not a directory
                    tries++;

                    //if I exceed the number of re-tries throw an exception and close the program
                    if(tries > maxTries)
                        throw ProtocolManagerException("Error from sent message", protocolManagerError::client);

                    //otherwise try to recover from the error
                    recoverFromError();
                    break;

                case errCode::exception:    //there was an exception in the server
                    throw ProtocolManagerException("Internal server error", protocolManagerError::internal);
                case errCode::auth: //authentication error
                case errCode::unexpected:   //unexpected message type
                    throw ProtocolManagerException("Unexpected error code", protocolManagerError::unexpected);
                default:
                    throw ProtocolManagerException("Unknown server error", protocolManagerError::unknown);
            }
        case messages::ServerMessage_Type_VER:
            throw ProtocolManagerException("Version not supported", protocolManagerError::version);
        default:
            //unrecognised message type
            std::cerr << "Unsupported message type" << std::endl;
            throw ProtocolManagerException("Unsupported message type error", client::protocolManagerError::unsupported);
    }

    //TODO maybe move this
    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    serverMessage.Clear();
}

/**
 * function to use to know if the protocol manager can send a message (it has a list of messages waiting to receive their response)
 *
 * @return true or false
 *
 * @author Michele Crepaldi s269951
 */
bool client::ProtocolManager::canSend() const {
    return (end+1)%size != start;
}

/**
 * function used to recover from errors or exceptions -> it re-sends all already sent messages which do not have had responses
 *
 * @throw SocketException in case it cannot write data to socket
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::recoverFromError() {
    for(int i = start; i != end; i = (i+1)%size){
        Event e = waitingForResponse[i];    //get the element from the queue

        if(e.getStatus() == FileSystemStatus::storeSent || e.getStatus() == FileSystemStatus::modifySent){
            //if the file to transfer is not present anymore in the filesystem or its hash is different from the one of
            //the file present in the filesystem then it means that the file was deleted or modified --> I can't send it anymore
            std::string ap = e.getElement().getAbsolutePath();
            if(!std::filesystem::directory_entry(ap).exists() || Directory_entry(path_to_watch, ap).getHash() != e.getElement().getHash())
                break;
        }

        //compose message based on event
        composeMessage(e);

        if(e.getStatus() == FileSystemStatus::storeSent || e.getStatus() == FileSystemStatus::modifySent){
            sendFile(e.getElement().getAbsolutePath()); //send the actual file
        }
    }
}

/**
 * function used to send a file to the server through messages
 *
 * @param path (absolute) path of the file to send
 * @param options
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::sendFile(const std::filesystem::path& path) {
    //initialize variables
    std::ifstream file;
    std::filesystem::directory_entry element(path);
    uint64_t bytesRead;
    char buff[MAXBUFFSIZE];

    //open input file
    file.open(path, std::ios::in | std::ios::binary);

    if(file.is_open()){
        while(file.read(buff, MAXBUFFSIZE)) //read file in MAXBUFFSIZE-wide blocks
            send_DATA(buff, file.gcount()); //send the block

        clientMessage.set_last(true);   //mark the last data block

        send_DATA(buff, file.gcount()); //send the last block
    }
    //TODO throw exception in case the file could not be opened

    //close the input file
    file.close();
}

/**
 * send the AUTH message to the server
 *
 * @param username username of the user who wants to authenticate
 * @param macAddress mac address of the client
 * @param password password of the user who wants to authenticate
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_AUTH(const std::string &username, const std::string &macAddress, const std::string &password){
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_AUTH);
    clientMessage.set_username(username);
    clientMessage.set_macaddress(macAddress);
    clientMessage.set_password(password);
    std::string client_temp = clientMessage.SerializeAsString();
    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}

/**
 * send the QUIT message to the server
 *
 * @author Michele Crepaldi s269551
 */
 /*
void client::ProtocolManager::send_QUIT(){
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_QUIT);

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}
*/

/**
 * send the PROB message to the server
 *
 * @param e Directory_entry to probe
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_PROB(Directory_entry &e){
    //create message
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_PROB);
    clientMessage.set_path(e.getRelativePath());
    clientMessage.set_hash(e.getHash().get().first,
                           e.getHash().get().second);

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}

/**
 * send the DELE message to the server
 *
 * @param e Directory_entry to delete
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_DELE(Directory_entry &e){
    //create message
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_DELE);
    clientMessage.set_path(e.getRelativePath());
    clientMessage.set_hash(e.getHash().get().first,
                           e.getHash().get().second);

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}

/**
 * send the STOR message to the server
 *
 * @param e Directory_entry to store
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_STOR(Directory_entry &e){
    //create message
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_STOR);
    clientMessage.set_path(e.getRelativePath());
    clientMessage.set_filesize(e.getSize());
    clientMessage.set_lastwritetime(e.getLastWriteTime());
    clientMessage.set_hash(e.getHash().get().first, e.getHash().get().second);

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}

/**
 * send the STOR message to the server
 *
 * @param buff data to send
 * @param len length of the data to send
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_DATA(char *buff, uint64_t len){
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_DATA);
    clientMessage.set_data(buff, len);    //insert file block in message

    //send message
    std::string client_message = clientMessage.SerializeAsString();
    s.sendString(client_message);

    //clear the message
    clientMessage.Clear();
}

/**
 * send the MKD message to the server
 *
 * @param e Directory_entry to create
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_MKD(Directory_entry &e){
    //create message
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_MKD);
    clientMessage.set_path(e.getRelativePath());
    clientMessage.set_lastwritetime(e.getLastWriteTime());

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}

/**
 * send the RMD message to the server
 *
 * @param e Directory_entry to remove
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_RMD(Directory_entry &e){
    //create message
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_RMD);
    clientMessage.set_path(e.getRelativePath());

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}

/**
 * function used to compose a clientMessage from an event
 *
 * @param e event to transform in message
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::composeMessage(Event &e) {
    //evaluate what to do
    if (e.getElement().is_regular_file()) {   //if the element is a file
        switch (e.getStatus()) {
            case FileSystemStatus::modified: //file modified
            case FileSystemStatus::created: //file created
                std::cout << "File created/modified: " << e.getElement().getAbsolutePath() << std::endl;

                send_PROB(e.getElement());
                break;

            case FileSystemStatus::deleted: //file deleted
                std::cout << "File deleted: " << e.getElement().getAbsolutePath() << std::endl;

                send_DELE(e.getElement());
                break;

            case FileSystemStatus::modifySent: //modify message sent
            case FileSystemStatus::storeSent: //store message sent
                std::cout << "Sending file: " << e.getElement().getAbsolutePath() << std::endl;

                send_STOR(e.getElement());
                break;

            default:    //I should never arrive here
                //TODO probably throw error
                std::cerr << "Error! Unknown file status." << std::endl;
        }
    } else { //if the element is a directory
        switch (e.getStatus()) {
            case FileSystemStatus::modified: //directory modified
            case FileSystemStatus::created: //directory created
                std::cout << "Directory created: " << e.getElement().getAbsolutePath() << std::endl;

                send_MKD(e.getElement());
                break;

            case FileSystemStatus::deleted: //directory deleted
                std::cout << "Directory deleted: " << e.getElement().getAbsolutePath() << std::endl;

                send_RMD(e.getElement());
                break;

            default:    //I should never arrive here
                //TODO probably throw error
                std::cerr << "Error! Unknown file status." << std::endl;
        }
    }
}
