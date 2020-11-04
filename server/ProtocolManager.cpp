//
// Created by michele on 21/10/2020.
//

#include <fstream>
#include <regex>
#include "ProtocolManager.h"

//TODO i will get these from config
#define PASSWORD_DATABASE_PATH "./serverfiles/passwordDB.sqlite"
#define DATABASE_PATH "./serverfiles/serverDB.sqlite"


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
 * send a QUIT response to the client
 *
 * @author Michele Crepaldi s269551
 */
 /*
void server::ProtocolManager::send_QUIT(){
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_QUIT);

    std::string tmp = serverMessage.SerializeAsString(); //crete string
    serverMessage.Clear();  //send response message
    s.sendString(tmp);
}
*/

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

    std::cout << "[PROB] - " << address << " (" << username << ") - " << path << std::endl;

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element
        send_SEND();
        return;
    }

    if(!el->second.is_regular_file()) {  //if the element is not a file
        //there is something with the same name which is not a file -> error
        send_ERR(errCode::notAFile);    //send error message with cause
        throw ProtocolManagerException("Probed something which is not a file.", protocolManagerError::client);      //TODO error code
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

    std::cout << "[STOR] - " << address << " (" << username << ") - " << expected.getRelativePath() << std::endl;

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

            if(clientMessage.version() != protocolVersion){
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
                //close the file
                out.close();

                //delete temporary file
                std::filesystem::remove(temporaryPath + tmpFileName);

                //no data message with last bool set was encountered before this message, error!
                send_ERR(errCode::unexpected);    //send error message with cause

                throw ProtocolManagerException("Unexpected message, DATA transfer was not done.", protocolManagerError::client);
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
            throw ProtocolManagerException("Stored file is different than expected.", protocolManagerError::client);
        }

        //If we are here then the file was successfully transferred and its copy on the server is as expected
        //so move then it can be moved to the final destination
        std::filesystem::rename(temporaryPath + tmpFileName, expected.getAbsolutePath());

        auto el = elements.find(expected.getRelativePath());
        if(el == elements.end()) {
            //add to the elements map
            elements.emplace(expected.getRelativePath(), expected);

            //add to db
            db->insert(username, mac, expected);
        }
        else{
            //update the element in elements map
            el->second = expected;

            //update into db
            db->update(username, mac, expected);
        }

        //TODO handle exception
        std::cout << "[DATA] - " << address << " (" << username << ") - " << expected.getRelativePath() << std::endl;

        //the file has been created
        send_OK(okCode::created);

        return;
    }

    //some error occurred
    send_ERR(errCode::exception);    //send error message with cause
    throw ProtocolManagerException("Could not open file or something else happened.", protocolManagerError::internal);
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

    std::cout << "[DELE] - " << address << " (" << username << ") - " << path << std::endl;

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element -> element does not exist, i don't have to remove it
        //the file did not exist, the result is the same
        send_OK(okCode::notThere);
        return;
    }

    //if it is not a file OR if the file hash does not correspond
    if(!el->second.is_regular_file() || el->second.getHash() != h){
        send_ERR(errCode::remove);    //send error message with cause
        throw ProtocolManagerException("Tried to remove something which is not a file or file hash does not correspond.", protocolManagerError::client);
    }

    //remove the file
    std::filesystem::remove(el->second.getAbsolutePath());   //true if the file was removed, false if it did not exist

    //remove element from db
    db->remove(username, mac, el->second.getRelativePath());

    //remove the element from the elements map
    elements.erase(el->second.getRelativePath());

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

    std::cout << "[MKD] - " << address << " (" << username << ") - " << path << std::endl;

    //check if the folder already exists
    bool create = !std::filesystem::directory_entry(basePath + path).exists();

    if(create)  //if the directory does not already exist
        std::filesystem::create_directories(basePath + path);    //create all the directories (if they do not already exist) up to the temporary path

    if(!std::filesystem::is_directory(basePath + path)){
        //the element is not a directory! (in case of a modify) -> error

        send_ERR(errCode::notADir);    //send error message with cause
        throw ProtocolManagerException("Tried to modify something which is not a directory.", protocolManagerError::client);
        return;
    }

    //create a Directory entry which represents the newly created folder (or to the already present one)
    Directory_entry newDir{basePath, std::filesystem::directory_entry(basePath + path)};
    //change last write time for the file
    newDir.set_time_to_file(lastWriteTime);

    auto el = elements.find(path);
    if(el == elements.end()) {
        //add to the elements map
        elements.emplace(newDir.getRelativePath(), newDir);

        //add to db
        db->insert(username, mac, newDir);
    }
    else{
        //update the element in elements map
        el->second = newDir;

        //update into db
        db->update(username, mac, newDir);
    }

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

    std::cout << "[RMD] - " << address << " (" << username << ") - " << path << std::endl;

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element -> element does not exist, i don't have to remove it
        //the file did not exist, the result is the same
        send_OK(okCode::notThere);
        return;
    }

    if(!el->second.is_directory()){
        //the element is not a directory! -> error
        send_ERR(errCode::notADir);    //send error message with cause
        throw ProtocolManagerException("Tried to remove something which is not a directory.", protocolManagerError::client);
    }

    //remove the directory
    //std::filesystem::remove(el->second.getAbsolutePath()); //it throws an exception if the dir is not empty
    //TODO descide if to use this instead which removes also subdirectories
    //int n_removed = std::filesystem::remove_all(el->second.getAbsolutePath());
    std::filesystem::recursive_directory_iterator iter{el->second.getAbsolutePath()};
    std::vector<Directory_entry> elementsToRemove;
    for(auto i: iter){
        //I save the elements in a vector before removing them in order to be sure to save them all
        elementsToRemove.emplace_back(basePath, i);
    }
    for(auto i: elementsToRemove){
        //if the element was already deleted by a previous call then this function returns 0,
        //otherwise it returns the number of deleted elements; in any case I want to remove the elements
        std::filesystem::remove_all(i.getAbsolutePath());

        //remove element from db
        db->remove(username, mac, i.getRelativePath());

        //remove the element from the elements map
        elements.erase(i.getRelativePath());
    }
    //this next call should remove just 1 element (the original directory to remove)
    std::filesystem::remove_all(el->second.getAbsolutePath());

    //remove element from db
    db->remove(username, mac, el->second.getRelativePath());

    //remove the element from the elements map
    elements.erase(el->second.getRelativePath());

    //the file has been removed
    send_OK(okCode::removed);
}

/**
 * function to quit the protocol
 *
 * @author Michele Crepaldi s269551
 */
 /*
void server::ProtocolManager::quit(){
    send_QUIT();    //send a quit to the client

    s.closeConnection();    //close the connection with the client
    std::cout << "Client " << address << " closed connection." << std::endl;

    throw ProtocolManagerException("", protocolManagerError::quit);
    //TODO verify if something else is needed
}
  */

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
server::ProtocolManager::ProtocolManager(Socket &s, const std::string &address, int ver, std::string basePath, std::string tempPath, int tempSize) :
    s(s),
    address(address),
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
        send_VER(); //send version message

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
            throw ProtocolManagerException( "Authentication Error",protocolManagerError::auth);
        }

        //the authentication was successful
        send_OK(okCode::authenticated);
    }
    else{
        //error, message not expected
        send_ERR(errCode::unexpected);    //send error message with cause
        throw ProtocolManagerException("Message Error, not expected.",protocolManagerError::unknown);
    }

    //set the base path
    std::stringstream tmp;
    tmp << basePath << "/" << username << "_" << std::regex_replace(mac, std::regex(":"), "-");
    basePath = tmp.str();

    std::cout << "[EVENT] - " << address << " - authenticated as " << username  << "@" << mac << std::endl;
};

/**
 * function to recover all the elements in the server db and populate the elements map (used at protocol manager startup)
 *
 * @throw DatabaseException from the db->forall function
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::recoverFromDB() {
    std::vector<Directory_entry> toUpdate, toDelete;

    std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> f;
    f = [this, &toUpdate, &toDelete](const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string& hash){
        auto current = Directory_entry(basePath, path, size, type, lastWriteTime, Hash(hash));

        //check if the file exists in the server filesystem (if the server has a copy of it)
        if(!std::filesystem::exists(current.getAbsolutePath())){
            //if the element does not exist then add it to the elements to delete from db
            toDelete.push_back(current);

            //then return (we don't want to add it to the elements map)
            return;
        }

        //the file exists, check if it corresponds to the one described by the database
        auto el = Directory_entry(basePath, current.getAbsolutePath());

        if(el.getType() != current.getType() || el.getSize() != current.getSize() ||
            el.getLastWriteTime() != current.getLastWriteTime() || el.getHash() != current.getHash()){

            //if there exists an element with the same name in the db but it is different from expected
            toUpdate.push_back(el);

            //then return (we don't want to add it to the elements map)
            return;
        }

        //if the file exists and it is the same as described in the db add it to the elements map
        elements.insert({path, current});
    };
    //apply the function for all the user's (and mac) elements in the db
    db->forAll(username, mac, f);

    //now update all the updated elements to db
    for(auto el: toUpdate){
        std::cerr << "[WARNING] Element " << el.getRelativePath() << " in " << basePath << " was modified offline!" << std::endl;
        db->update(username, mac, el);
    }

    //delete all the deleted elements from db
    for(auto el: toDelete){
        std::cerr << "[WARNING] Element " << el.getRelativePath() << " in " << basePath << " was removed offline!" << std::endl;
        db->remove(username, mac, el.getRelativePath());
    }
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
    if(protocolVersion != clientMessage.version()) {
        clientMessage.Clear();

        send_VER(); //send version message

        //throw exception
        throw ProtocolManagerException("Client is using a different version", protocolManagerError::version);
    }

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
            default:
                throw ProtocolManagerException("Unknown message type", protocolManagerError::unknown);
        }
    }
    catch (ProtocolManagerException &e) {
        switch (e.getCode()) {
            //in these 2 cases the errors may be temporary or just related to this particular message
            //so keep connection and skip the faulty message
            case server::protocolManagerError::unsupported: //a message from the client was of an unsupported type
            case server::protocolManagerError::client:  //there was an error in a message from the client
                //keep connection, skip this message
                return;

            //these next cases will be handled from main
            case server::protocolManagerError::auth:    //the current user failed authentication
            case server::protocolManagerError::version: //the current client uses a different version
            case server::protocolManagerError::internal: //there was an internal server error -> Fatal error
            case server::protocolManagerError::unknown: //there was an unknown error -> Fatal error
            default:
                throw ProtocolManagerException(e.what(),e.getCode());
        }
    }
    catch (SocketException &e){
        throw;  //rethrow exception (I don't want the general std::exception catch to catch it)
    }
    catch (server::PWD_DatabaseException &e){
        throw;  //rethrow exception (I don't want the general std::exception catch to catch it)
    }
    catch (server::DatabaseException &e){
        throw;  //rethrow exception (I don't want the general std::exception catch to catch it)
    }
    //TODO catch config exception
    catch (std::exception &e) {
        //internal error
        send_ERR(errCode::exception);    //send error message with cause

        throw ProtocolManagerException(e.what(),server::protocolManagerError::internal);
    }
}