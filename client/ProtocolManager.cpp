//
// Created by michele on 26/09/2020.
//

#include <fstream>
#include "ProtocolManager.h"

/**
 * constructor of protocol manager class
 *
 * @param s socket to read/write to/from
 * @param max max number of messages sent without response
 * @param ver verion of the protocol
 *
 * @author Michele Crepaldi s269551
 */
ProtocolManager::ProtocolManager(Socket &s, int max, int ver) : s(s), start(0), end(0), size(max), protocolVersion(ver) {
    waitingForResponse.resize(size+1);
}

/**
 * function used to authenticate a user to the service
 *
 * @param username username of the user
 * @param password password of the user
 *
 * @throw exception TODO in case of an authentication error (wrong username and password)
 * @throw exception TODO in case of a server error -> try again connection
 *
 * @author Michele Crepaldi s269551
 */
void ProtocolManager::authenticate(const std::string& username, const std::string& password) {
    //authenticate user
    //compute message
    clientMessage.set_type(messages::ClientMessage_Type_AUTH);
    clientMessage.set_username(username);
    clientMessage.set_password(password);
    std::string client_temp = clientMessage.SerializeAsString();
    //send message to server
    s.sendString(client_temp, 0);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();

    //get server response
    std::string server_temp = s.recvString(0);

    //parse server response
    serverMessage.ParseFromString(server_temp);
    std::cout << serverMessage.type();

    int code;

    switch (serverMessage.type()) {
        case messages::ServerMessage_Type_OK:
            //authentication ok
            code = serverMessage.code();
            std::cout << "authenticated, code " << code << std::endl;
            break;
        case messages::ServerMessage_Type_ERR:
            //retrieve error code
            code = serverMessage.code();
            std::cerr << "Error: code " << code << std::endl;

            //based on error code handle it
            //TODO handle error -> send exception to handle outside

            //stop the file system watcher and return
            //TODO --> fileWatcher_stop.store(true);
            return;
        case messages::ServerMessage_Type_VER:
            //server response contains the version to use
            //TODO handle version change and retry -> send exception to handle outside
            break;
        default:    //i should never arrive here
            std::cerr << "Unsupported message type" << std::endl;
            //TODO handle this protocol error and retry -> send exception to handle outside
            break;
    }

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    serverMessage.Clear();
}

/**
 * function to quit from service (connection)
 *
 * @author Michele Crepaldi s269551
 */
void ProtocolManager::quit() {
    //create message
    clientMessage.set_version(protocolVersion);
    clientMessage.set_type(messages::ClientMessage_Type_QUIT);

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp, 0);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();
}

/**
 * function used to send a message relative to an event to the server
 *
 * @param e event to create the message from
 *
 * @throw runtime error in case it cannot write data to socket
 *
 * @author Michele Crepaldi s269551
 */
void ProtocolManager::send(Event &e) {
    if (!e.getElement().is_regular_file() && !e.getElement().is_directory()) {  //if it is not a file nor a directory then return
        std::cerr << "change to an unsupported type." << std::endl;
        return;
    }

    //compose the message based on event
    composeMessage(e);

    //compute message
    std::string client_temp = clientMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp, 0);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    clientMessage.Clear();

    //save a copy of the event in a local list (to then wait for its respose)
    waitingForResponse[end] = e;
    end = (end+1)%size;
}

/**
 * function used to receive and process messages from server
 *
 * @throw runtime error in case it cannot read data from socket
 * @throw TODO in case of a repeated (more than X times) error for the same sent message
 *
 * @author Michele Crepaldi s269551
 */
void ProtocolManager::receive() {
    //get server response
    std::string server_temp = s.recvString(0);

    //parse server response
    serverMessage.ParseFromString(server_temp);

    Event e = waitingForResponse[start];

    //clear the server message
    serverMessage.Clear();

    //check version
    if(protocolVersion != serverMessage.version())
        std::cerr << "different protocol version!" << std::endl; //TODO throw protocol exception

    int code;
    switch (serverMessage.type()) {
        case messages::ServerMessage_Type_SEND:
            //only if the element is a file then it is right to send it
            if(e.getElement().is_regular_file() && e.getStatus() == FileSystemStatus::created){
                //send file

                //if(start != end) this condition is true if I receive something
                //pop the (create) event element (it was successful)
                start = (start+1)%size;

                //if the file to transfer is not present anymore in the filesystem or its hash is different from the one of the file present in the filesystem
                //then it means that the file was deleted or modified --> I can't send it anymore
                std::filesystem::path ap = e.getElement().getAbsolutePath();
                if(!std::filesystem::directory_entry(ap).exists() || Directory_entry(ap).getHash() != e.getElement().getHash())
                    break;

                //otherwise send file

                //save the send event in a local list (to then wait for its respose)
                Event tmp = Event(e.getElement(), FileSystemStatus::sent);
                waitingForResponse[end] = tmp;
                end = (end+1)%size;

                //compose the message based on send event
                composeMessage(tmp);

                //compute message
                std::string client_temp = clientMessage.SerializeAsString();

                //send message to server
                s.sendString(client_temp, 0);

                //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
                clientMessage.Clear();

                sendFile(e.getElement().getAbsolutePath(), 0);
                break;
            }
            //if I am here then I got a send message but the element is not a file so this is a protocol error
            std::cerr << "protocol error" << std::endl; //TODO handle protocol error
            break;
        case messages::ServerMessage_Type_OK:
            //if(start != end) this condition is true if I receive something
            //pop the event element (it was successful)
            start = (start+1)%size;

            std::cout << "Command for " << e.getElement().getAbsolutePath() << " has been sent to server: " << serverMessage.code() << std::endl;

            //TODO connect to database and update file/directory state
            break;
        case messages::ServerMessage_Type_ERR:
            //retrieve error code
            code = serverMessage.code();
            std::cerr << "Error: code " << code << std::endl;

            //based on error code handle it
            //TODO handle error and possibly retry
            recoverFromError();
            break;
        default:
            //unrecognised message type
            std::cerr << "Unsupported message type" << std::endl;
            //TODO handle this protocol error and retry
            break;
    }

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
bool ProtocolManager::canSend() const {
    return (end+1)%size != start;
}

/**
 * function used to recover from errors or exceptions -> it re-sends all already sent messages which do not have had responses
 *
 * @throw runtime error in case it cannot write data to socket
 *
 * @author Michele Crepaldi s269551
 */
void ProtocolManager::recoverFromError() {
    for(int i = start; i != end; i = (i+1)%size){
        Event &e = waitingForResponse[i];

        if(e.getStatus() == FileSystemStatus::sent){
            //if the file to transfer is not present anymore in the filesystem or its hash is different from the one of the file present in the filesystem
            //then it means that the file was deleted or modified --> I can't send it anymore
            std::filesystem::path ap = e.getElement().getAbsolutePath();
            if(!std::filesystem::directory_entry(ap).exists() || Directory_entry(ap).getHash() != e.getElement().getHash())
                break;
        }

        //compose message based on event
        composeMessage(e);

        //compute message
        std::string client_temp = clientMessage.SerializeAsString();

        //send message to server
        s.sendString(client_temp, 0);

        //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
        clientMessage.Clear();

        if(e.getStatus() == FileSystemStatus::sent){
            sendFile(e.getElement().getAbsolutePath(), 0);
        }
    }
}

/**
 * function used to compose a clientMessage from an event
 *
 * @param e event to transform in message
 *
 * @author Michele Crepaldi s269551
 */
void ProtocolManager::composeMessage(Event &e) {
    //evaluate what to do
    if (e.getElement().is_regular_file()) {   //if the element is a file
        switch (e.getStatus()) {
            case FileSystemStatus::created: //file created
                std::cout << "File created: " << e.getElement().getAbsolutePath() << "response from server: ";

                //create message
                clientMessage.set_version(protocolVersion);
                clientMessage.set_type(messages::ClientMessage_Type_PROB);
                clientMessage.set_path(e.getElement().getRelativePath());
                clientMessage.set_hash(e.getElement().getHash().getValue().first,
                                       e.getElement().getHash().getValue().second);
                break;

            case FileSystemStatus::deleted: //file deleted
                std::cout << "File deleted: " << e.getElement().getAbsolutePath() << std::endl;

                //create message
                clientMessage.set_version(protocolVersion);
                clientMessage.set_type(messages::ClientMessage_Type_DELE);
                clientMessage.set_hash(e.getElement().getHash().getValue().first,
                                       e.getElement().getHash().getValue().second);
                break;

            case FileSystemStatus::sent:
                //create message
                clientMessage.set_version(protocolVersion);
                clientMessage.set_type(messages::ClientMessage_Type_STOR);
                clientMessage.set_path(e.getElement().getRelativePath());
                clientMessage.set_filesize(e.getElement().getSize());
                clientMessage.set_lastwritetime(e.getElement().getLastWriteTime());
                clientMessage.set_hash(e.getElement().getHash().getValue().first,
                                       e.getElement().getHash().getValue().second);
                break;

            default:    //I should never arrive here
                std::cerr << "Error! Unknown file status." << std::endl;
        }
    } else { //if the element is a directory
        switch (e.getStatus()) {
            case FileSystemStatus::created: //directory created
                std::cout << "Directory created: " << e.getElement().getAbsolutePath() << std::endl;

                //create message
                clientMessage.set_version(protocolVersion);
                clientMessage.set_type(messages::ClientMessage_Type_MKD);
                clientMessage.set_path(e.getElement().getRelativePath());
                clientMessage.set_lastwritetime(e.getElement().getLastWriteTime());
                clientMessage.set_hash(e.getElement().getHash().getValue().first,
                                       e.getElement().getHash().getValue().second);
                break;

            case FileSystemStatus::deleted: //directory deleted
                std::cout << "Directory deleted: " << e.getElement().getAbsolutePath() << std::endl;

                //create message
                clientMessage.set_version(protocolVersion);
                clientMessage.set_type(messages::ClientMessage_Type_RMD);
                clientMessage.set_hash(e.getElement().getHash().getValue().first,
                                       e.getElement().getHash().getValue().second);
                break;

            default:    //I should never arrive here
                std::cerr << "Error! Unknown file status." << std::endl;
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
void ProtocolManager::sendFile(const std::filesystem::path& path, int options) {
    //initialize variables
    std::ifstream file;
    std::filesystem::directory_entry element(path);
    char buff[MAXBUFFSIZE];

    //open input file
    file.open(path, std::ios::in | std::ios::binary);

    if(file.is_open()){
        //read file in MAXBUFFSIZE-wide blocks
        while(file.read(buff, MAXBUFFSIZE)) {
            clientMessage.set_version(protocolVersion);
            clientMessage.set_type(messages::ClientMessage_Type_DATA);
            clientMessage.set_data(buff, file.gcount());    //insert file block in message

            //mark the last data block
            if(file.gcount() != MAXBUFFSIZE)
                clientMessage.set_last(true);

            //send message
            std::string client_message = clientMessage.SerializeAsString();
            s.sendString(client_message, options);

            //clear the message
            clientMessage.Clear();
        }
    }
    //TODO handle else

    //close the input file
    file.close();
}
