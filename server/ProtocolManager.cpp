//
// Created by michele on 21/10/2020.
//

#include <fstream>
#include <utility>
#include "ProtocolManager.h"

//TODO i will get these from config
#define PASSWORD_DATABASE_PATH "./serverfiles/passwordDB.sqlite"
#define DATABASE_PATH "./serverfiles/serverDB.sqlite"

#define MAXBUFFSIZE 1024

/**
 * send an OK response to the client with a code
 *
 * @param code OK code
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::send_OK(okCode code){
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_OK);
    serverMessage.set_code(static_cast<int>(code));

    std::string tmp = serverMessage.SerializeAsString();    //crete string
    s.sendString(tmp);      //send response message
    serverMessage.Clear();
}

/**
 * send a SEND response to the client with the path and hash of the file to send
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::send_SEND(){
    //the file was not there -> SEND message
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_SEND);
    serverMessage.set_path(clientMessage.path());

    //TODO evaluate if to add these
    //serverMessage.set_filesize(clientMessage.filesize());
    //serverMessage.set_lastwritetime(clientMessage.lastwritetime());
    //serverMessage.set_path(clientMessage.path());

    serverMessage.set_hash(clientMessage.hash());

    std::string tmp = serverMessage.SerializeAsString();    //crete string
    s.sendString(tmp);      //send response message
    serverMessage.Clear();
}

/**
 * send an ERR response to the client with an error code
 *
 * @param code protocolManagerError code
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::send_ERR(errCode code){
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_ERR);
    serverMessage.set_code(static_cast<int>(code));

    std::string tmp = serverMessage.SerializeAsString();    //crete string
    s.sendString(tmp);      //send response message
    serverMessage.Clear();
}

/**
 * send a VER response to the client with the version of this protocol Manager
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::send_VER(){
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_VER);
    serverMessage.set_newversion(protocolVersion);

    std::string tmp = serverMessage.SerializeAsString(); //crete string
    serverMessage.Clear();  //send response message
    s.sendString(tmp);
}

/**
 * error handler function for the Protocol Manager; it sends an error message and then throws an exception
 *
 * @param msg message to put in the exception
 * @param code to send in the message and to put in the exception
 *
 * @throw ProtocolManagerException
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::errorHandler(const std::string & msg, protocolManagerError code){

    //TODO decide what to do
    //throw exception
    throw ProtocolManagerException(msg, code);
}

/**
 * function to probe the server elements map for the file got in clientMessage
 *
 * @throw ProtocolManagerException if the element is not a file
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::probe() {
    std::string path = clientMessage.path();
    Hash h = Hash(clientMessage.hash());
    clientMessage.Clear();

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element
        send_SEND();
        return;
    }

    if(!el->second.is_regular_file()) {  //if the element is not a file
        //there is something with the same name which is not a file -> error
        send_ERR(errCode::notAFile);    //send error message with cause
        errorHandler("Internal Server Error", protocolManagerError::internal);      //TODO error code
        return;
    }

    if(el->second.getHash() != h) {   //if the file hash does not correspond then it means there exists a file which is different
        //we want to overwrite it so we say to the client to send the file
        send_SEND();
        return;
    }

    /* TODO evaluate if to add these
    if(el->second.getSize() != c.filesize())    //if the filesize does not correspond
        return false;

    if(el->second.getLastWriteTime() != c.lastwritetime())  //if the last write time does not correspond
        return false;
    */

    //the file has been found -> OK message
    send_OK(okCode::found);
}

/**
 * function to interpret the STOR message and to get all the DATA messages for a file;
 * it stores the file in a temporary directory with a temporary random name, when the
 * file transfer is done then it checks the file was correctly saved and moves it to
 * the final destination (overwriting any old existing file); then it updates the server db and elements map
 *
 * @throw ProtocolManagerException if the message version is not supported, if a non-DATA message is encountered
 * before the end of the file transmission, if an error occurred in opening the file and if the file after the end
 * of its transmission is not as expected
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::storeFile(){
    //create expected Directory entry element from the client message
    Directory_entry expected{basePath, clientMessage.path(), clientMessage.filesize(), "file",
                             clientMessage.lastwritetime(), Hash{clientMessage.hash()}};
    clientMessage.Clear();  //clear the client message

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
        bool loop;
        do{
            std::string message = s.recvString();
            clientMessage.ParseFromString(message);

            if(clientMessage.version() != protocol_version){
                //send the version message
                send_VER();

                //close the file
                out.close();

                //delete temporary file
                std::filesystem::remove(temporaryPath + tmpFileName);

                //throw exception
                throw ProtocolManagerException("Client is using a different version", protocolManagerError::version);
            }

            if(clientMessage.type() != messages::ClientMessage_Type_DATA){
                //no data message with last bool set was encountered before this message, error!
                send_ERR(errCode::unexpected);    //send error message with cause

                //close the file
                out.close();

                //delete temporary file
                std::filesystem::remove(temporaryPath + tmpFileName);

                errorHandler("Client Message Error", protocolManagerError::client);
            }

            loop = !clientMessage.last();  //is this the last data packet? if yes stop the loop

            std::string data = clientMessage.data();    //get the data from message
            out.write(data.data(), data.size());    //write it to file

            clientMessage.Clear();
        }
        while(loop);

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
            send_ERR(errCode::store);    //send error message with cause
            errorHandler("Client Message Error", protocolManagerError::client);
            return;
        }

        //If we are here then the file was successfully transferred and its copy on the server is as expected
        //so move then it can be moved to the final destination
        std::filesystem::rename(temporaryPath + tmpFileName, expected.getAbsolutePath());

        //add to the elements map
        elements.emplace(expected.getRelativePath(), expected);

        //add to db
        db->insert(username, mac, expected);
        //TODO handle exception

        //the file has been created
        send_OK(okCode::created);

        return;
    }

    //some error occurred
    send_ERR(errCode::store);    //send error message with cause
    errorHandler("Internal Server Error", protocolManagerError::internal);
}

/**
 * function to remove a file from the server filesystem, database and elements map
 *
 * @throw ProtocolManagerException if the element is not a file or if there is a file with the same path but the hash is different
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::removeFile(){
    const std::string path = clientMessage.path();
    Hash h{clientMessage.hash()};
    clientMessage.Clear();

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element -> element does not exist, i don't have to remove it
        //the file did not exist, the result is the same
        send_OK(okCode::notThere);
        return;
    }

    //if it is not a file OR if the file hash does not correspond
    if(!el->second.is_regular_file() || el->second.getHash() != h){
        send_ERR(errCode::remove);    //send error message with cause
        errorHandler("Internal Server Error", protocolManagerError::internal); //TODO decide if internal or client
        return;
    }

    //remove the file
    std::filesystem::remove(el->second.getAbsolutePath());   //true if the file was removed, false if it did not exist

    //remove the element from the elements map
    elements.erase(el->second.getAbsolutePath());

    //remove element from db
    db->remove(username, mac, el->second.getRelativePath());

    //the file has been removed
    send_OK(okCode::removed);
}

/**
 * function to create/(modify its last write time) a directory on the server filesystem and update the server db and elements map
 *
 * @throw ProtocolManagerException if there exists already an element with the same name and it is not a directory
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::makeDir(){
    const std::string path = clientMessage.path();
    const std::string lastWriteTime = clientMessage.lastwritetime();
    clientMessage.Clear();

    //check if the folder already exists
    bool create = !std::filesystem::directory_entry(basePath + path).exists();

    if(create)  //if the directory does not already exist
        std::filesystem::create_directories(basePath + path);    //create all the directories (if they do not already exist) up to the temporary path

    if(!std::filesystem::is_directory(basePath + path)){
        //the element is not a directory! -> error

        send_ERR(errCode::notADir);    //send error message with cause
        errorHandler("Internal Server Error", protocolManagerError::internal); //TODO decide if internal or client
        return;
    }

    //create a Directory entry which represents the newly created folder (or to the already present one)
    Directory_entry newDir{basePath, std::filesystem::directory_entry(basePath + path)};
    //change last write time for the file
    newDir.set_time_to_file(lastWriteTime);

    //add to the elements map
    elements.emplace(newDir.getRelativePath(), newDir);

    //add to db
    db->insert(username, mac, newDir);

    //the directory has been created/modified
    send_OK(okCode::created);
}

/**
 * function to remove a directory from the server filesystem, the server db and elements map
 *
 * @throw ProtocolManagerException if there exists an element with the same name but it is not a directory
 * @throw an exception in case the directory is not empty
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::removeDir(){
    const std::string path = clientMessage.path();
    clientMessage.Clear();

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element -> element does not exist, i don't have to remove it
        //the file did not exist, the result is the same
        send_OK(okCode::notThere);
        return;
    }

    if(!el->second.is_directory()){
        //the element is not a directory! -> error
        send_ERR(errCode::notADir);    //send error message with cause
        errorHandler("Internal Server Error", protocolManagerError::internal); //TODO decide if internal or client
        return;
    }

    //remove the directory
    std::filesystem::remove(el->second.getAbsolutePath()); //it throws an exception if the dir is not empty
    //TODO descide if to use this instead which removes also subdirectories
    //int n_removed = std::filesystem::remove_all(el->second.getAbsolutePath());

    //remove the element from the elements map
    elements.erase(el->second.getAbsolutePath());

    //remove element from db
    db->remove(username, mac, el->second.getRelativePath());

    //the file has been removed
    send_OK(okCode::removed);
}

/**
 * function to quit the protocol
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::quit(){
    //TODO decide what to do
}

/**
 * protocol manager constructor
 *
 * @param s socket associated to this thread
 * @param ver version of the protocol tu use
 * @param basePath base path for this client
 * @param tempPath temp path were to put temporary files
 * @param tempSize size of the temporary file name
 *
 * @throw may throw PWT_DatabaseException from PWD_Database
 * @throw may throw DatabaseException from Database
 *
 * @author Michele Crepaldi s269551
 */
server::ProtocolManager::ProtocolManager(Socket &s, int ver, std::string basePath, std::string tempPath, int tempSize) :
    s(s),
    protocolVersion(ver),
    basePath(std::move(basePath)),
    temporaryPath(std::move(tempPath)),
    tempNameSize(tempSize){

    //TODO get from config
    password_db = PWD_Database::getInstance(PASSWORD_DATABASE_PATH);
    db = Database::getInstance(DATABASE_PATH);
};

/**
 * function to authenticate a user, mac to this protocol manager
 *
 * @throw ProtocolManagerException if the version in the message is not supported, if the authentication was not successful
 * or if there was a not expected message
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::authenticate() {
    std::string message = s.recvString();
    clientMessage.ParseFromString(message);

    //check version
    if(protocolVersion != clientMessage.version()) {
        std::cerr << "different protocol version!" << std::endl; //TODO decide if to keep this and in case yes handle it
        send_VER();

        //throw exception
        throw ProtocolManagerException("Client is using a different version", protocolManagerError::version);
    }

    if(clientMessage.type() == messages::ClientMessage_Type_AUTH) {

        //get the information from the client message
        username = clientMessage.username();
        mac = clientMessage.macaddress();
        std::string password = clientMessage.password();

        //initialize hash maker with the password
        HashMaker hm{password};

        //get the salt and hash for the current user
        auto pair = password_db->getHash(username);
        std::string salt = pair.first;

        //append the salt to the password
        hm.update(salt);
        auto pwdHash = hm.get();    //get the computed hash

        if(pwdHash != pair.second){ //compare the computed hash with the user hash
            //if they are different then the password is not correct (authentication error)
            send_ERR(errCode::auth);    //send error message with cause
            errorHandler( "Authentication Error",protocolManagerError::auth); //TODO error code
        }

        //the authentication was successful
        send_OK(okCode::authenticated);
    }
    else{
        //error, message not expected
        send_ERR(errCode::unexpected);    //send error message with cause
        errorHandler("Message Error, not expected.",protocolManagerError::unknown);    //TODO error code
    }

    std::stringstream tmp;
    tmp << basePath << "/" << username << "_" << mac << "/";
    basePath = tmp.str();
};

/**
 * function to recover all the elements in the server db and populate the elements map (used at protocol manager startup)
 *
 * @throw DatabaseException from the db->forall function
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::recoverFromDB() {
    std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> f;
    f = [this](const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string& hash){
        auto element = Directory_entry(basePath, path, size, type, lastWriteTime, Hash(hash));
        elements.insert({path, element}); //TODO check base path
    };
    db->forAll(username, mac, f);
}

/**
 * function to receive a message from the client
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::receive(){
    //get client request
    std::string message = s.recvString();

    //parse client request
    clientMessage.ParseFromString(message);

    //check version
    if(protocolVersion != clientMessage.version())
        std::cerr << "different protocol version!" << std::endl; //TODO decide if to keep this and in case yes handle it

    //TODO go on
    try {
        std::string tmp;
        switch (clientMessage.type()) {
            case messages::ClientMessage_Type_PROB:

                probe();    //probe the elements list for the element in client message

                break;
            case messages::ClientMessage_Type_STOR: {

                storeFile();    //store the file in the server filesystem

                break;
            }
            case messages::ClientMessage_Type_DELE:

                removeFile();

                break;
            case messages::ClientMessage_Type_MKD:

                makeDir();

                break;
            case messages::ClientMessage_Type_RMD:

                removeDir();

                break;
            case messages::ClientMessage_Type_QUIT:

                quit();

                break;
            default:
                throw ProtocolManagerException("Unknown message type", protocolManagerError::unknown);
        }
    }
    catch (std::exception &e) {
        //internal error

        send_ERR(errCode::exception);    //send error message with cause

        //TODO decide what to do
    }
}