//
// Created by Michele Crepaldi s269551 on 21/10/2020
// Finished on 22/11/2020
// Last checked on 22/11/2020
//

#include "ProtocolManager.h"

#include <fstream>
#include <regex>

#include "../myLibraries/Message.h"
#include "../myLibraries/RandomNumberGenerator.h"
#include "../myLibraries/Validator.h"
#include "Config.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * ProtocolManager class methods
 */

/**
 * ProtocolManager constructor
 *
 * @param socket socket associated to this thread
 * @param address address of the connected client
 * @param ver version of the protocol tu use
 *
 * @author Michele Crepaldi s269551
 */
server::ProtocolManager::ProtocolManager(Socket &socket, std::string address, int ver) :
        _s(socket), //set socket
        _address(std::move(address)),   //set client address
        _protocolVersion(ver),  //set protocol version
        _recovered(false){  //set recovered member variable to false

    auto config = Config::getInstance();    //config object instance
    _basePath = config->getServerBasePath();    //get server base path
    _temporaryPath = config->getTempPath();     //get server temporary path
    _tempNameSize = config->getTmpFileNameSize();       //get temporary file name size
    _maxDataChunkSize = config->getMaxDataChunkSize();  //get max data chunk size

    _password_db = Database_pwd::getInstance(); //get database_pwd instance
    _db = Database::getInstance();              //get database instance
}

/**
 * ProtocolManager recover from database method.
 *  It is used to recover all the elements (for the current user-mac pair) in the server db and populate
 *  the elements map
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::recoverFromDB() {
    std::vector<Directory_entry> toUpdate;  //list of all Directory_entry elements to update
    std::vector<Directory_entry> toDelete;  //list of all Directory_entry elements to delete

    //function to be used for each element of the db
    std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &,
                        const std::string &)> f;

    f = [this, &toUpdate, &toDelete](const std::string &path, const std::string &type, uintmax_t size,
                                     const std::string &lastWriteTime, const std::string& hash){

        //current Directory_entry element
        auto current = Directory_entry(_userPath, path, size, type, lastWriteTime, Hash(hash));

        //check if the file exists in the server filesystem (if the server has a copy of it)
        if(!std::filesystem::exists(current.getAbsolutePath())){
            //if the element does not exist add it to the elements to delete from db
            toDelete.push_back(std::move(current));

            return;
        }
        //otherwise

        //if the file exists, check if it corresponds to the one described by the database

        //effective Directory_entry element on filesystem
        auto effective = Directory_entry(_userPath, current.getAbsolutePath());

        //if the effective element found on filesystem is different from the current one
        if(effective.getType() != current.getType() || effective.getSize() != current.getSize() ||
           effective.getLastWriteTime() != current.getLastWriteTime() || effective.getHash() != current.getHash()){

            //if there exists an element with the same name in the db but it is different from expected

            //add it to the elements to update into the db
            toUpdate.push_back(std::move(effective));

            return;
        }
        //otherwise

        //if the file exists and it is the same as described in the db

        //add it to the elements map
        _elements.emplace(path, std::move(current));
    };

    //apply the function for all the user's (and mac) elements in the db
    _db->forAll(_username, _mac, f);

    //for all the elements to update
    for(auto el: toUpdate){
        Message::print(std::cerr, "WARNING", el.getRelativePath() + " in " + _userPath,
                       "was modified offline!");

        //update the element on database
        _db->update(_username, _mac, el);

        //insert the element into the elements map
        _elements.emplace(el.getRelativePath(), std::move(el));
    }

    //for all the elements to delete
    for(auto el: toDelete){
        Message::print(std::cerr, "WARNING", el.getRelativePath() + " in " + _userPath,
                       "was removed offline!");

        //delete the element from database
        _db->remove(_username, _mac, el.getRelativePath());
    }

    //set _recovered boolean member variable to inform that the recovery has been completed
    _recovered = true;
}

/**
 * ProtocolManager authenticate method.
 *  It is used to authenticate a client (username-mac)
 *
 * @throws ProtocolManagerException:
 *  <b>version</b> if the client message uses a different version
 * @throws ProtocolManagerException:
 *  <b>client</b> if there are error in the client message (validation failed)
 * @throws ProtocolManagerException:
 *  <b>auth</b> if the authentication failed
 * @throws ProtocolManagerException:
 *  <b>unexpected</b> if an unexpected client message was received
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::authenticate() {
    //receive a message from client

    std::string message = _s.recvString();      //client message
    _clientMessage.ParseFromString(message);    //get clientMessage protobuf parsing the clinet message

    //check clientMessage version
    if(_protocolVersion != _clientMessage.version()) {
        //it is more efficient to clear the clientMessage protobuf than creating a new one
        _clientMessage.Clear();

        //send version message to the client
        _send_VER();

        throw ProtocolManagerException("Client is using a different version", ProtocolManagerError::version);
    }

    //if the clientMessage type is AUTH
    if(_clientMessage.type() == messages::ClientMessage_Type_AUTH) {

        //get the information from the client message

        _username = _clientMessage.username();  //get username from clientMessage
        _mac = _clientMessage.macaddress();     //get mac address from clientMessage
        std::string password = _clientMessage.password();   //get password from clientMessage

        //it is more efficient to clear the clientMessage protobuf than creating a new one
        _clientMessage.Clear();


        //validate path got from clientMessage
        if(!Validator::validateUsername(_username))
            throw ProtocolManagerException("Username validation failed", ProtocolManagerError::client);

        //validate last write time got from clientMessage
        if(!Validator::validateMacAddress(_mac))
            throw ProtocolManagerException("Mac address validation failed", ProtocolManagerError::client);

        //validate last write time got from clientMessage
        if(!Validator::validatePassword(password))
            throw ProtocolManagerException("Password validation failed", ProtocolManagerError::client);


        //initialize hash maker with the password
        HashMaker hm{password};

        //get the salt and password hash for the current user

        //(salt,hash) pair for the current user
        auto pair = _password_db->getHash(_username);

        //user's salt
        std::string salt = pair.first;

        //update the hash maker with the user's salt (effectively appending the salt to the password)
        hm.update(salt);

        //computed password hash
        auto pwdHash = hm.get();

        //compare the computed password hash with the user hash
        if(pwdHash != pair.second){
            //if they are different then the password is not correct (authentication error)

            //send error message with cause to the client
            _send_ERR(ErrCode::auth);

            throw ProtocolManagerException("Authentication Error", ProtocolManagerError::auth);
        }

        //the authentication was successful

        //send ok message to the client
        _send_OK(OkCode::authenticated);
    }
    else{   //if the clientMessage type is not AUTH

        //error, message not expected

        //send error message with cause to the client
        _send_ERR(ErrCode::unexpected);

        //throw exception
        throw ProtocolManagerException("Message Error, not expected.", ProtocolManagerError::unexpected);
    }

    //set the server base path (where to put the backed-up files)

    std::stringstream tmp;
    tmp << _basePath << "/" << _username << "_" << std::regex_replace(_mac, std::regex(":"), "-");
    _userPath = tmp.str();

    Message::print(std::cout, "EVENT", _address, "authenticated as " + _username + "@" + _mac);
}

/**
 * ProtocolManager receive method.
 *  It is used to receive a message from the client
 *
 * @throws ProtocolManagerException:
 *  <b>version</b> if the client message uses a different version
 * @throws ProtocolManagerException:
 *  <b>unexpected</b> if an unexpected client message was received
 * @throws ProtocolManagerException:
 *  <b>internal</b> if there is some un-handled exception
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::receive(){
    //receive a message from client

    std::string message = _s.recvString();      //client message
    _clientMessage.ParseFromString(message);    //get clientMessage protobuf parsing the message

    //check clientMessage version
    if(_protocolVersion != _clientMessage.version()) {
        //it is more efficient to clear the clientMessage protobuf than creating a new one
        _clientMessage.Clear();

        //send version message to the client
        _send_VER();

        throw ProtocolManagerException("Client is using a different version", ProtocolManagerError::version);
    }

    try {
        //switch on client message type
        switch (_clientMessage.type()) {
            case messages::ClientMessage_Type_PROB:
                _probe();   //probe elements map for the file in client message
                break;

            case messages::ClientMessage_Type_STOR:
                _storeFile();   //store file in the server filesystem, db and elements map
                break;

            case messages::ClientMessage_Type_DELE:
                _removeFile();  //remove file from server filesystem, db and elements map
                break;

            case messages::ClientMessage_Type_MKD:
                _makeDir(); //create directory on server filesystem, db and elements map
                break;

            case messages::ClientMessage_Type_RMD:
                _removeDir();   //remove directory from server filesystem, db and elements map
                break;

            case messages::ClientMessage_Type_RETR:
                _retrieveUserData();    //retrieve the user's backed-up files and send it to client
                break;

            case messages::ClientMessage_Type_NOOP:
            case messages::ClientMessage_Type_DATA:
            case messages::ClientMessage_Type_AUTH:
            default:
                //unexpected message types

                //send error message with cause to the client
                _send_ERR(ErrCode::unexpected);

                throw ProtocolManagerException("Unexpected message type", ProtocolManagerError::unexpected);
        }
    }
    catch (ProtocolManagerException &e) {
        //switch on error code
        switch (e.getCode()) {
            //in these 2 cases the errors may be temporary or just related to this particular message
            //so keep connection and skip the faulty message
            case server::ProtocolManagerError::unexpected:
            case server::ProtocolManagerError::client:
                //keep connection, skip this message.
                Message::print(std::cerr, "WARNING", "ProtocolManager error from the received message",
                               "will now skip faulty message");
                return;

                //these next cases will be handled from main
            case server::ProtocolManagerError::auth:
            case server::ProtocolManagerError::version:
            case server::ProtocolManagerError::internal:
            default:
                //re-throw exception
                throw;
        }
    }
    catch (SocketException &e){
        //re-throw exception (I don't want the general std::exception catch to catch it)
        throw;
    }
    catch (server::DatabaseException_pwd &e){
        //re-throw exception (I don't want the general std::exception catch to catch it)

        //send error message with cause to the client
        _send_ERR(ErrCode::exception);

        throw;
    }
    catch (server::DatabaseException &e){
        //re-throw exception (I don't want the general std::exception catch to catch it)

        //send error message with cause to the client
        _send_ERR(ErrCode::exception);

        throw;
    }
    catch (server::ConfigException &e){
        //re-throw exception (I don't want the general std::exception catch to catch it)

        //send error message with cause to the client
        _send_ERR(ErrCode::exception);

        throw;
    }
    catch (std::exception &e) {
        //internal server error

        //send error message with cause to the client
        _send_ERR(ErrCode::exception);

        throw ProtocolManagerException(e.what(),server::ProtocolManagerError::internal);
    }
}


/**
 * ProtocolManager send serverMessage method.
 *  It will send the serverMessage and then clear it
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_serverMessage(){
    //string representation of the serverMessage protobuf
    std::string tmp = _serverMessage.SerializeAsString();

    //send response message
    _s.sendString(tmp);

    //it is more efficient to clear the serverMessage protobuf than creating a new one
    _serverMessage.Clear();
}

/**
 * ProtocolManager send OK message method.
 *  It will set the serverMessage protobuf version, type and code and then send it
 *
 * @param code OK code
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_OK(OkCode code){
    _serverMessage.set_version(_protocolVersion);
    _serverMessage.set_type(messages::ServerMessage_Type_OK);

    //set ok code
    _serverMessage.set_code(static_cast<int>(code));

    _send_serverMessage();
}

/**
 * ProtocolManager send SEND message method.
 *  It will set the serverMessage protobuf version, type, path and hash and then send it
 *  (path and hash are taken from last received clientMessage)
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_SEND(const std::string &path, const std::string &hash){
    _serverMessage.set_version(_protocolVersion);
    _serverMessage.set_type(messages::ServerMessage_Type_SEND);

    //set path and hash to the value in the last clientMessage received
    _serverMessage.set_path(path);
    _serverMessage.set_hash(hash);

    _send_serverMessage();
}

/**
 * ProtocolManager send ERR message method.
 *  It will set the serverMessage protobuf version, type and code and then send it
 *
 * @param code protocolManagerError code
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_ERR(ErrCode code){
    _serverMessage.set_version(_protocolVersion);
    _serverMessage.set_type(messages::ServerMessage_Type_ERR);

    //set error code
    _serverMessage.set_code(static_cast<int>(code));

    _send_serverMessage();
}

/**
 * ProtocolManager send VER message method.
 *  It will set the serverMessage protobuf version, type and new version and then send it
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_VER(){
    _serverMessage.set_version(_protocolVersion);
    _serverMessage.set_type(messages::ServerMessage_Type_VER);

    //set new version as the one supported by the protocol manager
    _serverMessage.set_newversion(_protocolVersion);

    _send_serverMessage();
}

/**
 * ProtocolManager file probe method.
 *  Used to probe the server elements map for the file got in PROB clientMessage
 *
 * @throws ProtocolManagerException:
 *  <b>client</b> if there were errors in the client message (validation failed)
 * @throws ProtocolManagerException:
 *  <b>client</b> if the element is not a file
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_probe() {
    //recover user data from database (if not already done previously)
    if(!_recovered)
        recoverFromDB();

    std::string path = _clientMessage.path();                   //file relative path
    std::string lastWriteTime = _clientMessage.lastwritetime(); //file last write time
    Hash h = Hash(_clientMessage.hash());                       //file hash

    //it is more efficient to clear the clientMessage protobuf than creating a new one
    _clientMessage.Clear();


    //validate path got from clientMessage
    if(!Validator::validatePath(path))
        throw ProtocolManagerException("Path validation failed", ProtocolManagerError::client);

    //validate last write time got from clientMessage
    if(!Validator::validateLastWriteTime(lastWriteTime))
        throw ProtocolManagerException("Last write time validation failed", ProtocolManagerError::client);


    Message::print(std::cout, "PROB", _address + " (" + _username + "@" + _mac + ")", path);

    //(string, Directory_entry) pair corresponding to the relative path
    auto el = _elements.find(path);

    //if i cannot find the element
    if(el == _elements.end()) {

        //tell the client to send it
        _send_SEND(path, h.str());
        return;
    }
    //otherwise

    //if the element is not a file
    if(!el->second.is_regular_file()) {
        //there is something with the same name which is not a file -> send error message with cause
        _send_ERR(ErrCode::notAFile);
        throw ProtocolManagerException("Probed something which is not a file.", ProtocolManagerError::client);
    }

    //if the file hash does not correspond -> a file with the same name exists but it is different
    if(el->second.getHash() != h) {

        //we want to overwrite it so tell the client to send it
        _send_SEND(path, h.str());
        return;
    }

    //if the file last write time does not correspond
    //-> the file exists and it is the same, but it has a different last write time
    //(maybe on client it was just 'touched')
    if(el->second.getLastWriteTime() != lastWriteTime){

        //update the last write time to be as the one found in clientMessage
        el->second.set_time_to_file(lastWriteTime);
    }

    //the file has been found and is the same -> send OK message
    _send_OK(OkCode::found);
}

/**
 * ProfocolManager file store method.
 *  Used to interpret the STOR message got from client and to get all the DATA messages for a file;
 *  it stores the file in a temporary directory with a temporary random name, and when the file transfer is done
 *  then it checks the file was correctly saved and moves it to the final destination
 *  (overwriting any old existing file); then it updates the server db and elements map
 *
 * @throws ProtocolManagerException:
 *  <b>version</b> if the DATA message version is not supported (should not happen, but it checks it anyway)
 * @throws ProtocolManagerException:
 *  <b>client</b> if there were errors in the client message (validation failed)
 * @throws ProtocolManagerException:
 *  <b>unexpected</b> if the DATA transfer was unexpectedly interrupted by another message type
 * @throws ProtocolManagerException:
 *  <b>client</b> if the transferred file is different from its description found in STOR message (so if either its
 *  size or hash is different)
 * @throws ProtocolManagerException:
 *  <b>internal</b> if an error occurred in creating the file
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_storeFile(){
    //recover user data from database (if not already done previously)
    if(!_recovered)
        recoverFromDB();

    std::string path = _clientMessage.path();                   //file relative path
    uintmax_t size = _clientMessage.filesize();                 //file size
    std::string lastWriteTime = _clientMessage.lastwritetime(); //file last write time
    Hash h = Hash(_clientMessage.hash());                       //file hash

    //it is more efficient to clear the clientMessage protobuf than creating a new one
    _clientMessage.Clear();


    //validate path got from clientMessage
    if(!Validator::validatePath(path))
        throw ProtocolManagerException("Path validation failed", ProtocolManagerError::client);

    //validate last write time got from clientMessage
    if(!Validator::validateLastWriteTime(lastWriteTime))
        throw ProtocolManagerException("Last write time validation failed", ProtocolManagerError::client);


    //expected Directory entry element (got from the client message)
    Directory_entry expected{_userPath, path, size, "file", lastWriteTime, h};

    Message::print(std::cout, "STOR", _address + " (" + _username + "@" + _mac + ")",
                   expected.getRelativePath());

    //create a random temporary name for the file

    RandomNumberGenerator rng;  //random number generator
    std::string tmpFileName = "/" + rng.getHexString(_tempNameSize) + ".tmp";   //temporary file name

    //check if the temporary folder already exists
    bool tmpPathExists = std::filesystem::exists(_temporaryPath);

    //if the temporary directory does not already exist
    if(!tmpPathExists)
        //create all the directories (that do not already exist) up to the temporary path
        std::filesystem::create_directories(_temporaryPath);

    std::ofstream temporaryFile; //temporary file

    //create the temporary file
    temporaryFile.open(_temporaryPath + tmpFileName, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if(temporaryFile.is_open()){
        try {
            bool loop = true;
            while(loop){
                //receive message from client

                std::string message = _s.recvString();      //message got from client

                //convert message to clientMessage protobuf
                _clientMessage.ParseFromString(message);

                //check message version
                if (_clientMessage.version() != _protocolVersion) { //if the version is different
                    //it is more efficient to clear the clientMessage protobuf than creating a new one
                    _clientMessage.Clear();

                    //send the version message
                    _send_VER();

                    //close the temporary file
                    temporaryFile.close();

                    //delete the temporary file
                    std::filesystem::remove(_temporaryPath + tmpFileName);

                    throw ProtocolManagerException("Client is using a different version",
                                                   ProtocolManagerError::version);
                }

                //check message type, it has to be DATA type
                if (_clientMessage.type() != messages::ClientMessage_Type_DATA) {   //if it is not of type DATA
                    //it is more efficient to clear the clientMessage protobuf than creating a new one
                    _clientMessage.Clear();

                    //close the temporary file
                    temporaryFile.close();

                    //delete the temporary file
                    std::filesystem::remove(_temporaryPath + tmpFileName);

                    //no DATA message with "last" boolean set was encountered before this message, error!

                    //send error message with cause to client
                    _send_ERR(ErrCode::unexpected);

                    throw ProtocolManagerException("Unexpected message, DATA transfer was not done.",
                                                   ProtocolManagerError::unexpected);
                }

                //is this the last data packet? if yes stop the loop
                loop = !_clientMessage.last();

                //data got from the clientMessage protobuf
                std::string data = _clientMessage.data();

                //write the data to temporary file
                temporaryFile.write(data.data(), data.size());

                //it is more efficient to clear the clientMessage protobuf than creating a new one
                _clientMessage.Clear();
            }
        }
        //in case of socket exceptions while transferring the file I need to delete the temporary file
        catch (SocketException &e) {
            //close the temporary file
            temporaryFile.close();

            //delete the temporary file
            std::filesystem::remove(_temporaryPath + tmpFileName);

            //re-throw the exception
            throw;
        }

        //close the temporary file
        temporaryFile.close();

        //Directory entry which represents the newly created file
        Directory_entry newFile{_temporaryPath, std::filesystem::directory_entry(_temporaryPath + tmpFileName)};

        //change last write time for the temporary file to what was expected
        newFile.set_time_to_file(expected.getLastWriteTime());

        //temporary file hash
        Hash hash = newFile.getHash();

        //check if the newly created (temporary) file properties match the expected ones
        if(newFile.getSize() != expected.getSize() || hash != expected.getHash() ||
                newFile.getLastWriteTime() != expected.getLastWriteTime()) {

            //if the temporary file is not as we expected

            //delete the temporary file
            std::filesystem::remove(_temporaryPath + tmpFileName);

            //send error message with cause to client
            _send_ERR(ErrCode::store);

            throw ProtocolManagerException("Stored file is different than expected.",
                                           ProtocolManagerError::client);
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
        //any lastWriteTime modification to that directory will be requested explicitly by the client,
        //so we want to keep the same time before and after the file move

        //Directory entry representing the file parent directory
        Directory_entry parent;

        //only if the parent path is different from server base path get parent Directory entry (otherwise we
        //have problems getting relative path
        if(parentPath.string() != _userPath)
            parent = {_userPath, parentPath.string()};

        //If we are here then the file was successfully transferred and its copy on the server is as expected
        //it can be moved to the final destination
        std::filesystem::rename(_temporaryPath + tmpFileName, expected.getAbsolutePath());

        //if the parent directory is not the server base path
        if(parentPath.string() != _userPath)
            //reset the parent directory lastWriteTime
            parent.set_time_to_file(parent.getLastWriteTime());

        Message::print(std::cout, "DATA", _address + " (" + _username + "@" + _mac + ")",
                       expected.getRelativePath());

        //update the elements map and db

        //(string,Directory_entry) pair corresponding to the expected relative path
        auto el = _elements.find(expected.getRelativePath());

        //if the element was not found
        if(el == _elements.end()) {
            //add the expected file to the db
            _db->insert(_username, _mac, expected);

            //add the expected file to the elements map
            _elements.emplace(expected.getRelativePath(), std::move(expected));
        }
        else{
            //update into db
            _db->update(_username, _mac, expected);

            //update the expected file element in the elements map
            el->second = std::move(expected);
        }

        //send ok message to client
        _send_OK(OkCode::created);
        return;
    }

    //if I am here some error occurred

    //send error message with cause to client
    _send_ERR(ErrCode::exception);

    throw ProtocolManagerException("Could not open file or something else happened.",
                                   ProtocolManagerError::internal);
}

/**
 * ProtocolManager remove file method.
 *  It is used to remove a file from the server filesystem, database and elements map
 *
 * @throw ProtocolManagerException if the element is not a file or if there is a file with the same path but the hash is different
 *
 * @throws ProtocolManagerException:
 *  <b>client</b> if there were errors in the client message (validation failed)
 * @throws ProtocolManagerException:
 *  <b>client</b> if the element to remove is not a file or the file hash does not correspond
 * @throws ProtocolManagerException:
 *  <b>internal</b> if it could not remove the file
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_removeFile(){
    //recover user data from database (if not already done previously)
    if(!_recovered)
        recoverFromDB();

    std::string path = _clientMessage.path();     //file relative path
    Hash h{_clientMessage.hash()};                      //file Hash

    //it is more efficient to clear the clientMessage protobuf than creating a new one
    _clientMessage.Clear();


    //validate path got from clientMessage
    if(!Validator::validatePath(path))
        throw ProtocolManagerException("Path validation failed", ProtocolManagerError::client);


    Message::print(std::cout, "DELE", _address + " (" + _username + "@" + _mac + ")", path);

    //(string,Directory_entry) pair corresponding to the relative path got from clientMessage
    auto el = _elements.find(path);

    //if i cannot find the element, i don't have to remove it (the result is the same)
    if(el == _elements.end()) {

        //send ok message to client
        _send_OK(OkCode::notThere);
        return;
    }
    //otherwise

    //if the file does not exist on filesystem, i don't have to remove it (the result is the same)
    if(!el->second.exists()){
        //remove the file from db
        _db->remove(_username, _mac, el->second.getRelativePath());

        //remove the file from the elements map
        _elements.erase(el->second.getRelativePath());

        //send ok message to client
        _send_OK(OkCode::notThere);
        return;
    }
    //otherwise

    //if it is not a file OR if the file hash does not correspond
    if(!el->second.is_regular_file() || el->second.getHash() != h){

        //send error message with cause to client
        _send_ERR(ErrCode::remove);

        throw ProtocolManagerException("Tried to remove something which is not a file or file hash does"
                                       "not correspond.", ProtocolManagerError::client);
    }

    //get the file parent path

    //file parent path
    std::filesystem::path parentPath = std::filesystem::path(el->second.getAbsolutePath()).parent_path();

    //save the lastWriteTime of the destination folder (parent directory) before removing the file,
    //any lastWriteTime modification to that directory will be requested explicitly by the client,
    //so we want to keep the same time before and after the file remove

    //Directory entry representing the file parent directory
    Directory_entry parent;

    //only if the parent path is different from server base path get parent Directory entry (otherwise we
    //have problems getting relative path
    if(parentPath.string() != _userPath)
        parent = {_userPath, parentPath.string()};

    //remove the file
    if(!std::filesystem::remove(el->second.getAbsolutePath()))
        //if it could not remove the file throw an exception
        throw ProtocolManagerException("Could not remove a file", ProtocolManagerError::internal);

    //if the parent directory is not the server base path
    if(parentPath.string() != _userPath)
        //reset the parent directory lastWriteTime
        parent.set_time_to_file(parent.getLastWriteTime());

    //remove the file from db
    _db->remove(_username, _mac, el->second.getRelativePath());

    //remove the file from the elements map
    _elements.erase(el->second.getRelativePath());

    //send ok message to client
    _send_OK(OkCode::removed);
}

/**
 * ProtocolManager make directory method.
 *  It is used to create/(modify last write time of) a directory on the server filesystem and update the server db
 *  and elements map
 *
 * @throws ProtocolManagerException:
 *  <b>client</b> if there were errors in the client message (validation failed)
 * @throws ProtocolManagerException:
 *  <b>client</b> if there exists already an element with the same name and it is not a directory
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_makeDir(){
    //recover user data from database (if not already done previously)
    if(!_recovered)
        recoverFromDB();

    std::string path = _clientMessage.path();                     //dir relative path
    std::string lastWriteTime = _clientMessage.lastwritetime();   //dir last write time

    //it is more efficient to clear the clientMessage protobuf than creating a new one
    _clientMessage.Clear();


    //validate path got from clientMessage
    if(!Validator::validatePath(path))
        throw ProtocolManagerException("Path validation failed", ProtocolManagerError::client);

    //validate last write time got from clientMessage
    if(!Validator::validateLastWriteTime(lastWriteTime))
        throw ProtocolManagerException("Last write time validation failed", ProtocolManagerError::client);


    Message::print(std::cout, "MKD", _address + " (" + _username + "@" + _mac + ")", path);

    //(string,Directory_entry) pair corresponding to the relative path got from clientMessage
    auto el = _elements.find(path);

    //if I found the element AND the element exists AND the element has the same last write time as expected
    if(el != _elements.end() && el->second.exists() && el->second.getLastWriteTime() == lastWriteTime){

        //send ok message to client
        _send_OK(OkCode::created);
        return;
    }
    //otherwise

    //get the dir parent path

    //dir parent path
    auto parentPath = std::filesystem::path(_userPath + path).parent_path();

    //check if the parent folder already exists
    bool parentExists = std::filesystem::exists(parentPath.string());

    //Directory entry representing the dir parent folder
    Directory_entry parent;

    //if the parent directory exists and the parent path is different from server base path
    if(parentExists && parentPath.string() != _userPath)
        //save the lastWriteTime of the destination folder (parent directory) before creating the directory,
        //any lastWriteTime modification to that directory will be requested explicitly by the client,
        //so we want to keep the same time before and after the directory create
        parent = Directory_entry{_userPath, parentPath.string()};

    //if the directory does not already exist
    if(!std::filesystem::exists(_userPath + path))
        //create all the directories (that do not already exist) up to the path
        std::filesystem::create_directories(_userPath + path);

    //in case the it existed, if it is not a directory
    if(!std::filesystem::is_directory(_userPath + path)){

        //send error message with cause to client
        _send_ERR(ErrCode::notADir);

        throw ProtocolManagerException("Tried to modify something which is not a directory.",
                                       ProtocolManagerError::client);
    }

    //Directory entry which represents the newly created folder (or the already present one)
    Directory_entry newDir{_userPath, std::filesystem::directory_entry(_userPath + path)};

    //change last write time for the directory
    newDir.set_time_to_file(lastWriteTime);

    //if the parent directory already existed before (and it is not the base path)
    if(parentExists && parentPath.string() != _userPath)
        //reset the parent directory lastWriteTime
        parent.set_time_to_file(parent.getLastWriteTime());

    //if I could not find the directory in the elements map
    if(el == _elements.end()) {
        //add the newly created directory to the db
        _db->insert(_username, _mac, newDir);

        //add the newly created directory to the elements map
        _elements.emplace(newDir.getRelativePath(), std::move(newDir));
    }
    else{
        //update the directory in the db
        _db->update(_username, _mac, newDir);

        //update the directory in the elements map
        el->second = std::move(newDir);
    }

    //send ok message to client
    _send_OK(OkCode::created);
}

/**
 * ProtocolManager remove directory method.
 *  It is used to remove a directory from the server filesystem, server db and elements map
 *
 * @throws ProtocolManagerException:
 *  <b>client</b> if there were errors in the client message (validation failed)
 * @throws ProtocolManagerException:
 *  <b>client</b> if the element to remove is not a directory
 * @throws ProtocolManagerException:
 *  <b>internal</b> if it could not remove the directory
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_removeDir(){
    //recover user data from database (if not already done previously)
    if(!_recovered)
        recoverFromDB();

    std::string path = _clientMessage.path();   //dir relative path

    //it is more efficient to clear the clientMessage protobuf than creating a new one
    _clientMessage.Clear();


    //validate path got from clientMessage
    if(!Validator::validatePath(path))
        throw ProtocolManagerException("Path validation failed", ProtocolManagerError::client);


    Message::print(std::cout, "RMD", _address + " (" + _username + "@" + _mac + ")", path);

    //(string,Directory_entry) pair corresponding to the relative path got from clientMessage
    auto el = _elements.find(path);

    //if i cannot find the element, I don't have to remove it (the result is the same)
    if(el == _elements.end()) {

        //send ok message to client
        _send_OK(OkCode::notThere);
        return;
    }
    //otherwise

    //if the directory does not exist in filesystem, I don't have to remove it (the result is the same)
    if(!el->second.exists()) {
        //remove element from db
        _db->remove(_username, _mac, el->second.getRelativePath());

        //remove the element from the elements map
        _elements.erase(el->second.getRelativePath());

        //send ok message to client
        _send_OK(OkCode::notThere);
        return;
    }
    //otherwise

    //if it is not a directory
    if(!el->second.is_directory()){

        //send error message with cause to client
        _send_ERR(ErrCode::notADir);

        throw ProtocolManagerException("Tried to remove something which is not a directory.",
                                       ProtocolManagerError::client);
    }
    //otherwise

    auto dirToRemove = el->second;

    //get the dir parent path

    //dir parent path
    auto parentPath = std::filesystem::path(_userPath + path).parent_path();

    //save the lastWriteTime of the destination folder (parent directory) before removing the directory,
    //any lastWriteTime modification to that directory will be requested explicitly by the client,
    //so we want to keep the same time before and after the directory remove

    //Directory entry representing the dir parent directory
    Directory_entry parent;

    //only if the parent path is different from server base path get parent Directory entry (otherwise we
    //have problems getting relative path)
    if(parentPath.string() != _userPath)
        parent = {_userPath, parentPath.string()};

    //recursive directory iterator for the directory to remove
    std::filesystem::recursive_directory_iterator iter{dirToRemove.getAbsolutePath()};

    //vector of all elements to remove
    std::vector<Directory_entry> elementsToRemove;

    //save the elements in a vector before removing them in order to be sure to remove them all
    for(auto i: iter)
        elementsToRemove.emplace_back(_userPath, i);

    //remove the directory (and all its subdirectories and files) from filesystem
    if(std::filesystem::remove_all(dirToRemove.getAbsolutePath()) == 0)
        throw ProtocolManagerException("Could not remove an element.", ProtocolManagerError::internal);

    //then remove each sub element from both the db and elements map
    for(auto i: elementsToRemove){
        //remove element from the db
        _db->remove(_username, _mac, i.getRelativePath());

        //remove the element from the elements map
        _elements.erase(i.getRelativePath());
    }

    //remove the directory from the db
    _db->remove(_username, _mac, dirToRemove.getRelativePath());

    //remove the directory from the elements map
    _elements.erase(dirToRemove.getRelativePath());

    //if the parent directory is not the base path
    if(parentPath.string() != _userPath)
        //reset the parent directory lastWriteTime
        parent.set_time_to_file(parent.getLastWriteTime());

    //send ok message to client
    _send_OK(OkCode::removed);
}

/**
 * ProtocolManager send MKD message method.
 *  It will set the serverMessage protobuf version, type, path and last write time and then send it
 *
 * @param e Directory_entry to create
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_MKD(const std::string &path, Directory_entry &e){
    _serverMessage.set_version(_protocolVersion);
    _serverMessage.set_type(messages::ServerMessage_Type_MKD);

    //set path and last write time
    _serverMessage.set_path(path);
    _serverMessage.set_lastwritetime(e.getLastWriteTime());

    _send_serverMessage();
}

/**
 * ProtocolManager send STOR message method.
 *  It will set the serverMessage protobuf version, type, path, file size, last write time and hash and then send it
 *
 * @param path relative path of the file on client (with respect to the client base path)
 * @param element Directory_entry element (file) to store on client
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_STOR(const std::string &path, Directory_entry &element){
    _serverMessage.set_version(_protocolVersion);
    _serverMessage.set_type(messages::ServerMessage_Type_STOR);

    //set the path, filesize, last write time and hash
    _serverMessage.set_path(path);
    _serverMessage.set_filesize(element.getSize());
    _serverMessage.set_lastwritetime(element.getLastWriteTime());
    _serverMessage.set_hash(element.getHash().get().first, element.getHash().get().second);

    _send_serverMessage();
}

/**
 * ProtocolManager send DATA message method.
 *  It will set the serverMessage protobuf version, type, data and then send it
 *
 * @param buff buffer to the data to send
 * @param len length of the data to send
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_send_DATA(char *buff, uint64_t len){
    _serverMessage.set_version(_protocolVersion);
    _serverMessage.set_type(messages::ServerMessage_Type_DATA);

    //set the data
    _serverMessage.set_data(buff, len);

    _send_serverMessage();
}

/**
 * ProtocolManager retrieve user data method.
 *  It is used to retrieve the user's requested data and send it to the client
 *
 * @throws ProtocolManagerException:
 *  <b>client</b> if there were errors in the client message (validation failed)
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::_retrieveUserData(){

    //client macAddress (if present, otherwise a default "" value is passed)
    std::string macAddr = _clientMessage.macaddress();

    bool retrAll = _clientMessage.all(); //retrieve all boolean

    //it is more efficient to clear the clientMessage protobuf than creating a new one
    _clientMessage.Clear();


    //vector with all the elements to send
    std::vector<std::pair<std::string, Directory_entry>> toSend{};

    //if retrieve all boolean is true, the user requested all its files (independently from the machine mac address)
    if(retrAll){

        Message::print(std::cout, "RETR", _address + " (" + _username + "@" + _mac + ")", "All files");

        auto macs = _db->getAllMacAddresses(_username);   //all the user's mac addresses

        //for all the mac addresses
        for(std::string m: macs){

            //compose the relative root directory name from username and mac; this will be the folder in which the
            //files and directories associated to this username-mac pair will be saved on client

            std::stringstream tmp;
            tmp << "/" << _username << "_" << std::regex_replace(m, std::regex(":"), "-");

            //relative root directory name (from username-mac pair)
            std::string relativeRoot = tmp.str();

            //function to be used for each user's element in the db (for mac address m)
            std::function<void(const std::string &, const std::string &, uintmax_t, const std::string &,
                    const std::string &)> f;

            f = [this, &toSend, &relativeRoot, &m](const std::string &path, const std::string &type, uintmax_t size,
                    const std::string &lastWriteTime, const std::string& hash){

                //current element
                auto current = Directory_entry(_basePath + relativeRoot, path, size, type,
                                               lastWriteTime, Hash(hash));

                //pre-append the relative root (username_mac) to the element relative path
                //and insert the pair into the toSend map
                toSend.emplace_back(m, std::move(current));
            };

            //perform the function f on all the user elements (with mac m)
            _db->forAll(_username, m, f);
        }
    }
    else{   //if retrieve all boolean is false, macAddress will contain the specific mac address to use

        //if the macAddr got from clientMessage is empty
        if(macAddr.empty()){

            //send the error message with cause to the client
            _send_ERR(ErrCode::retrieve);

            throw ProtocolManagerException("Error in client message", ProtocolManagerError::client);
        }

        //validate mac address got from clientMessage
        if(!Validator::validateMacAddress(macAddr))
            throw ProtocolManagerException("Mac address validation failed", ProtocolManagerError::client);

        Message::print(std::cout, "RETR", _address + " (" + _username + "@" + _mac + ")", "mac = "
                        + macAddr);

        //compose the relative root directory name from username and mac; this will be the folder in which the
        //files and directories associated to this username-mac pair will be saved on client

        std::stringstream tmp;
        tmp << "/" << _username << "_" << std::regex_replace(macAddr, std::regex(":"), "-");

        //relative root directory name (from username-mac pair)
        std::string relativeRoot = tmp.str();

        //function to be used for each user's element in the db (for mac address m)
        std::function<void(const std::string &, const std::string &, uintmax_t, const std::string &,
                           const std::string &)> f;

        f = [this, &toSend, &relativeRoot, &macAddr](const std::string &path, const std::string &type, uintmax_t size,
                                           const std::string &lastWriteTime, const std::string& hash){

            //current element
            auto current = Directory_entry(_basePath + relativeRoot, path, size, type, lastWriteTime, Hash(hash));

            //pre-append the relative root (username_mac) to the element relative path
            //and insert the pair into the toSend map
            toSend.emplace_back(macAddr, std::move(current));
        };

        //perform the function f on all the user-macAddress elements
        _db->forAll(_username, macAddr, f);
    }

    //now, in toSend map I will have all the elements to send to the client

    //for all the pairs in the toSend map
    for(auto pair: toSend){
        std::string currentMac = pair.first;
        Directory_entry current = std::move(pair.second);

        //if the file to transfer does not exist anymore in the filesystem
        if(!std::filesystem::exists(current.getAbsolutePath())) {

            //remove it from the db and just go to the next element (return)
            _db->remove(_username, currentMac, current.getRelativePath());

            //print a warning and skip it
            Message::print(std::cerr, "WARNING",_address + " (" + _username + "@" + _mac + ")",
                           current.getRelativePath() + " was removed offline. It will not be sent");
            continue;
        }

        //if it is a directory
        if(current.is_directory()){
            //compose the relative root directory name from username and mac; this will be the folder in which the
            //files and directories associated to this username-mac pair will be saved on client

            std::stringstream tmp;
            tmp << "/" << _username << "_" << std::regex_replace(currentMac, std::regex(":"), "-");

            //relative root directory name (from username-mac pair)
            std::string relativeRoot = tmp.str();

            Message::print(std::cout, "RETR-MKD", _address + " (" + _username + "@" + _mac + ")",
                           relativeRoot + current.getRelativePath());

            //send MKD message to client
            _send_MKD(relativeRoot + current.getRelativePath(), current);
        }
        //if it is a file
        else if(current.is_regular_file()) {
            //send the file to the client
            _sendFile(current, currentMac);
        }
        //else the element is not supported so just skip it
    }

    //send ok message to the client, this will signal the end of transmissions
    _send_OK(OkCode::retrieved);
}

/**
 * ProtocolManager send file method.
 *  It is used to send a single file to the client
 *
 * @param element Directory_entry element to send
 * @param macAddr macAddress related to this element (together with _username)
 *
 * @throws ProtocolManagerException:
 *  <b>internal</b> if the file could not be opened
 */
void server::ProtocolManager::_sendFile(Directory_entry &element,
                                        std::string &macAddr) {

    std::ifstream file;             //input file
    char buff[_maxDataChunkSize];   //buffer used to read from file and send to socket

    //compose the relative root directory name from username and mac; this will be the folder in which the
    //files and directories associated to this username-mac pair will be saved on client

    std::stringstream tmp;
    tmp << "/" << _username << "_" << std::regex_replace(macAddr, std::regex(":"), "-");

    //relative root directory name (from username-mac pair)
    std::string relativeRoot = tmp.str();

    //directory entry representing the effective file present on filesystem (with same name)
    Directory_entry effective{_basePath + relativeRoot, element.getAbsolutePath()};

    //if the file hash got from db is different from the one of the file present in the filesystem
    if(element.getHash() != effective.getHash()){
        //the file was modified on server!

        //remove it from the db, since the file saved does not exist anymore
        _db->remove(_username, macAddr, element.getRelativePath());

        //print a warning and skip it
        Message::print(std::cerr, "WARNING",_address + " (" + _username + "@" + _mac + ")",
                       element.getRelativePath() + " was modified offline. It will not be sent");

        return;
    }

    Message::print(std::cout, "RETR-STOR", _address + " (" + _username + "@" + _mac + ")",
                   relativeRoot + element.getRelativePath());

    //send STOR message to the client
    _send_STOR(relativeRoot + element.getRelativePath(), element);

    //open input file
    file.open(element.getAbsolutePath(), std::ios::in | std::ios::binary);

    if(file.is_open()){

        //read file in maxDataChunkSize-wide blocks
        while(file.read(buff, _maxDataChunkSize))
            _send_DATA(buff, file.gcount());    //send the block

        _serverMessage.set_last(true);  //mark the last data block

        _send_DATA(buff, file.gcount()); //send the last block

        file.close();   //close the input file
    }
    else    //if file could not be opened
        throw ProtocolManagerException("Could not open file", ProtocolManagerError::internal);
}