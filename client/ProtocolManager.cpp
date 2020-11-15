//
// Created by michele on 26/09/2020.
//

#include <fstream>
#include "ProtocolManager.h"
#include "../myLibraries/TS_Message.h"

#define TEMP_RELATIVE_PATH "/temp"


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
    max_data_chunk_size = config->getMaxDataChunkSize();
    tempNameSize = config->getTmpFileNameSize();

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

    int code;
    switch (serverMessage.type()) {
        case messages::ServerMessage_Type_OK:
            //authentication ok
            code = serverMessage.code();
            switch(static_cast<client::okCode>(code)) {
                case okCode::authenticated: //user authentication successful
                    TS_Message::print(std::cout, "AUTH", "Authenticated");
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
            TS_Message::print(std::cout, "VER", "Change version to", std::to_string(serverMessage.newversion()));
            //for future use (not implemented now, to implement when a new version is written)
            //for now terminate the program
            throw ProtocolManagerException("Version not supported", protocolManagerError::version);

        default: //i should never arrive here
            TS_Message::print(std::cerr, "ERROR", "Unsupported message type");
            throw ProtocolManagerException("Unsupported message type error", protocolManagerError::unsupported);
    }

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    //serverMessage.Clear();
}

/**
 * function to get the number of messages waiting for a server response
 *
 * @return # of messages waiting for a response
 *
 * @author Michele Crepaldi s269551
 */
int client::ProtocolManager::waitingForResponses() const{
    return (size + end - start) % size;
}

/**
 * function to know if the Protocol Manager is waiting for server responses or not
 *
 * @return whether the Protocol Manager is waiting for server responses or not
 *
 * @author Michele Crepaldi s269551
 */
bool client::ProtocolManager::isWaiting() const{
    return start != end;
}

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
        TS_Message::print(std::cerr, "WARNING", "Change to an unsupported type", e.getElement().getRelativePath());
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

                //if(start != end), this condition is true if I receive something //TODO remove this line?
                //pop the (create) event element (it was successful)
                start = (start+1)%size;
                //reset the tries variable (i popped a message)
                tries = 0;

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

                sendFile(e.getElement());
                break;
            }
            //if I am here then I got a send message but the element is not a file so this is a protocol error (I should never get here)
            //TODO throw exception
            TS_Message::print(std::cerr, "ERROR", "protocol error");
            break;
        case messages::ServerMessage_Type_OK:
            //if(start != end) this condition is true if I receive something
            //pop the event element (it was successful)
            start = (start+1)%size;
            //reset the tries variable (i popped a message)
            tries = 0;

            code = serverMessage.code();

            switch(static_cast<client::okCode>(code)) {
                case okCode::found:     //probe file found the file
                    TS_Message::print(std::cout, "SUCCESS", "PROB", e.getElement().getRelativePath());
                    break;

                case okCode::created:   //file/directory create was successful
                    TS_Message::print(std::cout, "SUCCESS", "STOR/MKD", e.getElement().getRelativePath());
                    break;

                case okCode::notThere:  //file/directory remove did not find the file/directory (the end effect is the same as a successful remove)
                case okCode::removed:   //file/directory remove was successful
                    TS_Message::print(std::cout, "SUCCESS", "DELE/RMD", e.getElement().getRelativePath());
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

                    TS_Message::print(std::cerr, "WARNING", "Message error", "Will now retry");

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
            break;
        case messages::ServerMessage_Type_VER:
            throw ProtocolManagerException("Version not supported", protocolManagerError::version);
        default:
            //unrecognised message type
            TS_Message::print(std::cerr, "ERROR", "Unsupported message type", "");
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
            sendFile(e.getElement()); //send the actual file
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
void client::ProtocolManager::sendFile(Directory_entry &element) {
    //initialize variables
    std::ifstream file;
    char buff[max_data_chunk_size];

    //if the file to transfer is not present anymore in the filesystem or its hash is different from the one of the file present in the filesystem
    //then it means that the file was deleted or modified --> I can't send it anymore
    Directory_entry effective{path_to_watch, element.getAbsolutePath()};
    if(!effective.exists() || effective.getHash() != element.getHash())
        throw ProtocolManagerException("File not present anymore", protocolManagerError::client);

    //open input file
    file.open(element.getAbsolutePath(), std::ios::in | std::ios::binary);

    if(file.is_open()){
        int64_t totRead = 0;
        TS_Message message{"SENDING", "Sending file:", element.getRelativePath()};
        std::cout << message;

        while(file.read(buff, max_data_chunk_size)) { //read file in max_data_chunk_size-wide blocks
            totRead += file.gcount();
            send_DATA(buff, file.gcount()); //send the block
            message.update(std::floor((float)100.0 * totRead/element.getSize()));
            std::cout << message;
        }

        clientMessage.set_last(true);   //mark the last data block

        totRead += file.gcount();
        send_DATA(buff, file.gcount()); //send the block
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
        throw ProtocolManagerException("Could not open file", protocolManagerError::client);
}

//TODO comment
void client::ProtocolManager::retrieveFiles(const std::string &macAddress, bool all, const std::string &destFolder){

    send_RETR(macAddress, all);

    std::string tempDir = destFolder  + TEMP_RELATIVE_PATH;
    int tempFileNameSize = tempNameSize;

    while(true) {
        //get server response
        std::string server_temp = s.recvString();

        //parse server response
        serverMessage.ParseFromString(server_temp);

        //check version
        if (protocolVersion != serverMessage.version())
            throw ProtocolManagerException("Different protocol version!",
                                           client::protocolManagerError::version); //TODO decide if to keep this

        int code;
        switch (serverMessage.type()) {
            case messages::ServerMessage_Type_MKD:  //create a directory
                makeDir(destFolder);
                break;
            case messages::ServerMessage_Type_STOR: //store a file from server
                storeFile(destFolder, tempDir, tempFileNameSize);
                break;
            case messages::ServerMessage_Type_SEND:
            case messages::ServerMessage_Type_OK:   //end of the server messages -> return

                code = serverMessage.code();
                serverMessage.Clear();

                switch(static_cast<client::okCode>(code)) {
                    case okCode::retrieved:
                        TS_Message::print(std::cout, "SUCCESS", "RETR", "Saved all your data to " + destFolder);
                        return;

                    case okCode::found:     //probe file found the file
                    case okCode::created:   //file/directory create was successful
                    case okCode::notThere:  //file/directory remove did not find the file/directory (the end effect is the same as a successful remove)
                    case okCode::removed:   //file/directory remove was successful
                    case okCode::authenticated: //user authentication successful
                    default:
                        throw ProtocolManagerException("Unexpected OK code", protocolManagerError::unexpected);

                }

            case messages::ServerMessage_Type_ERR:
                //retrieve error code
                code = serverMessage.code();
                serverMessage.Clear();

                //based on error code handle it
                switch (static_cast<client::errCode>(code)) {
                    case errCode::retrieve: //there was an error in the message sent to the server (such as all to false and mac address not initialized)
                        throw  ProtocolManagerException("Client error", protocolManagerError::client);
                    case errCode::exception:    //there was an exception in the server
                        throw ProtocolManagerException("Internal server error", protocolManagerError::internal);
                    case errCode::auth: //authentication error
                    case errCode::notAFile: //error in PROB
                    case errCode::store:    //error in STOR
                    case errCode::remove:   //error in DELE
                    case errCode::notADir:  //error in RMD/MKD
                    case errCode::unexpected:   //unexpected message type
                        throw ProtocolManagerException("Unexpected error code", protocolManagerError::unexpected);
                    default:
                        throw ProtocolManagerException("Unknown server error", protocolManagerError::unknown);
                }
            case messages::ServerMessage_Type_VER:
                throw ProtocolManagerException("Version not supported", protocolManagerError::version);
            default:
                //unrecognised message type
                TS_Message::print(std::cerr, "ERROR", "Unsupported message type", "");
                throw ProtocolManagerException("Unsupported message type error",
                                               client::protocolManagerError::unsupported);
        }
    }
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
 * send the RETR message to the server
 *
 * @param username username to retrieve data of
 * @param macAddress mac address to retrieve the data about
 * @param all if to retrieve all the data about the user or only the ones related to the user-mac pair
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::send_RETR(const std::string &macAddress, bool all){
    //create message
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_RETR);

    clientMessage.set_macaddress(macAddress);
    clientMessage.set_all(all);

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
                TS_Message::print(std::cout, "EVENT", "File created/modified", e.getElement().getRelativePath());

                send_PROB(e.getElement());
                break;

            case FileSystemStatus::deleted: //file deleted
                TS_Message::print(std::cout, "EVENT", "File deleted", e.getElement().getRelativePath());

                send_DELE(e.getElement());
                break;

            case FileSystemStatus::modifySent: //modify message sent
            case FileSystemStatus::storeSent: //store message sent
                //TS_Message::print(std::cout, "SENDING", "Sending file", e.getElement().getRelativePath());

                send_STOR(e.getElement());
                break;

            default:    //I should never arrive here
                //TODO probably throw error
                TS_Message::print(std::cerr, "ERROR", "Unknown file status.");
        }
    } else { //if the element is a directory
        switch (e.getStatus()) {
            case FileSystemStatus::modified: //directory modified
            case FileSystemStatus::created: //directory created
                TS_Message::print(std::cout, "EVENT", "Directory created/modified", e.getElement().getRelativePath());

                send_MKD(e.getElement());
                break;

            case FileSystemStatus::deleted: //directory deleted
                TS_Message::print(std::cout, "EVENT", "Directory deleted", e.getElement().getRelativePath());

                send_RMD(e.getElement());
                break;

            default:    //I should never arrive here
                //TODO probably throw error
                TS_Message::print(std::cerr, "ERROR", "Unknown file status.");
        }
    }
}

/**
 * function to interpret the STOR message and to get all the DATA messages for a file;
 * it stores the file in a temporary directory with a temporary random name, when the
 * file transfer is done then it checks the file was correctly saved and moves it to
 * the final destination (overwriting any old existing file); then it updates the server db and elements map
 *
 * @param destFolder destination folder where to put files
 * @param temporaryPath temporary path where to put temporary files
 * @param tempNameSize file name size for temporary files
 *
 * @throw ProtocolManagerException if the message version is not supported, if a non-DATA message is encountered
 * before the end of the file transmission, if an error occurred in opening the file and if the file after the end
 * of its transmission is not as expected
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::storeFile(const std::string &destFolder, const std::string &temporaryPath, int tempNameSize){
    //create expected Directory entry element from the client message
    Directory_entry expected{destFolder, serverMessage.path(), serverMessage.filesize(), "file",
                             serverMessage.lastwritetime(), Hash{serverMessage.hash()}};
    serverMessage.Clear();  //clear the client message

    TS_Message::print(std::cout, "STOR", expected.getRelativePath(), "in " + destFolder);

    //create a random temporary name for the file
    RandomNumberGenerator rng;
    std::string tmpFileName = "/" + rng.getHexString(tempNameSize) + ".tmp";

    //check if the temporary folder already exists
    bool create = !std::filesystem::directory_entry(temporaryPath).exists();

    if(create)  //if the directory does not already exist
        std::filesystem::create_directories(temporaryPath);    //create all the directories (if they do not already exist) up to the temporary path

    std::ofstream out;
    out.open( temporaryPath + tmpFileName, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if(out.is_open()){
        try {
            bool loop;
            int64_t totRead = 0;
            TS_Message msg{"RECV", "Receiving file:", expected.getRelativePath()};
            std::cout << msg;

            do {
                std::string message = s.recvString();
                serverMessage.ParseFromString(message);

                if (serverMessage.version() != protocolVersion) {

                    //close the file
                    out.close();

                    //delete temporary file
                    std::filesystem::remove(temporaryPath + tmpFileName);

                    //throw exception
                    throw ProtocolManagerException("Server is using a different version",
                                                   protocolManagerError::version);
                }

                if (serverMessage.type() != messages::ServerMessage_Type_DATA) {
                    //close the file
                    out.close();

                    //delete temporary file
                    std::filesystem::remove(temporaryPath + tmpFileName);

                    throw ProtocolManagerException("Unexpected message, DATA transfer was not done.",
                                                   protocolManagerError::internal);
                }
                loop = !serverMessage.last();  //is this the last data packet? if yes stop the loop

                std::string data = serverMessage.data();    //get the data from message
                out.write(data.data(), data.size());    //write it to file

                totRead += data.size();
                if(totRead != 0)
                    msg.update(std::floor((float)100.0 * totRead/expected.getSize()));
                else
                    msg.update(100);

                std::cout << msg << std::endl;

                serverMessage.Clear();
            } while (loop);
        }
        catch (SocketException &e) {    //in case of socket exceptions while transferring the file I need to delete the temporary file
            //close the file
            out.close();

            //delete temporary file
            std::filesystem::remove(temporaryPath + tmpFileName);

            //rethrow the exception
            throw;
        }

        //close the file
        out.close();

        //create a Directory entry which represents the newly created file
        Directory_entry newFile{temporaryPath, std::filesystem::directory_entry(temporaryPath + tmpFileName)};
        //change last write time for the file
        newFile.set_time_to_file(expected.getLastWriteTime());
        Hash hash = newFile.getHash();

        //if the written file has a different size from what expected OR if the calculated hash is different from what expected OR if the lastWriteTime for the file was not successfully set
        if(newFile.getSize() != expected.getSize() || hash != expected.getHash() || newFile.getLastWriteTime() != expected.getLastWriteTime()) {
            std::filesystem::remove(temporaryPath + tmpFileName);    //delete temporary file

            //some error occurred
            throw ProtocolManagerException("Stored file is different than expected.", protocolManagerError::internal);
        }

        //get the parent path
        std::filesystem::path parentPath = std::filesystem::path(expected.getAbsolutePath()).parent_path();

        //check if the parent folder already exists
        create = !std::filesystem::directory_entry(parentPath).exists();

        if(create)  //if the directory does not already exist
            std::filesystem::create_directories(parentPath);    //create all the directories (if they do not already exist) up to the parent path

        //take the lastWriteTime of the destination folder before moving the file,
        //any lastWriteTime modification will be requested by the client so we want to keep the same time
        Directory_entry parent{destFolder, parentPath.string()};

        //If we are here then the file was successfully transferred and its copy on the server is as expected
        //so move then it can be moved to the final destination
        std::filesystem::rename(temporaryPath + tmpFileName, expected.getAbsolutePath());

        //reset the parent directory lastWriteTime
        if(parent.getAbsolutePath() != destFolder)
            parent.set_time_to_file(parent.getLastWriteTime());

        //TODO evaluate if to handle socket exception here
        return;
    }

    //some error occurred
    throw ProtocolManagerException("Could not open file or something else happened.", protocolManagerError::client);
}

/**
 * function to create/(modify its last write time) a directory on the client filesystem
 *
 * @throw ProtocolManagerException if there exists already an element with the same name and it is not a directory
 *
 * @param destFolder destination folder where to create the directory
 *
 * @author Michele Crepaldi s269551
 */
void client::ProtocolManager::makeDir(const std::string &destFolder){
    const std::string path = serverMessage.path();
    const std::string lastWriteTime = serverMessage.lastwritetime();
    serverMessage.Clear();

    TS_Message::print(std::cout, "MKD", path, "in " + destFolder);

    //get the parent path (to then get its lastWriteTime in order to keep it the same after the dir create
    auto parentPath = std::filesystem::path(destFolder + path).parent_path();
    auto parExists = std::filesystem::directory_entry(parentPath.string()).exists();
    Directory_entry parent;

    if(parExists)   //if the parent directory exists
        //take the lastWriteTime of the destination folder before creating the directory,
        //any lastWriteTime modification will be requested by the client so we want to keep the same time
        parent = Directory_entry{destFolder, parentPath.string()};

    if(!std::filesystem::directory_entry(destFolder + path).exists())  //if the directory does not already exist
        std::filesystem::create_directories(destFolder + path);    //create all the directories (if they do not already exist) up to the temporary path

    if(!std::filesystem::is_directory(destFolder + path)){
        //the element is not a directory! (in case of a modify) -> error

        throw ProtocolManagerException("Tried to modify something which is not a directory.", protocolManagerError::internal);
    }

    //create a Directory entry which represents the newly created folder (or to the already present one)
    Directory_entry newDir{destFolder, std::filesystem::directory_entry(destFolder + path)};

    //change last write time for the file
    newDir.set_time_to_file(lastWriteTime);

    if(parExists && parent.getAbsolutePath() != destFolder)   //if the parent directory already existed before (and it is not the base path)
        //reset the parent directory lastWriteTime
        parent.set_time_to_file(parent.getLastWriteTime());
}