//
// Created by michele on 21/10/2020.
//

#include <fstream>
#include <regex>
#include <utility>
#include "ProtocolManager.h"
#include "../myLibraries/TS_Message.h"


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
 * send the STOR message to the server
 *
 * @param e Directory_entry to store
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::send_STOR(const std::string &path, Directory_entry &e){
    //create message
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_STOR);
    serverMessage.set_path(path);
    serverMessage.set_filesize(e.getSize());
    serverMessage.set_lastwritetime(e.getLastWriteTime());
    serverMessage.set_hash(e.getHash().get().first, e.getHash().get().second);

    //compute message
    std::string client_temp = serverMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    serverMessage.Clear();
}

/**
 * send the STOR message to the server
 *
 * @param buff data to send
 * @param len length of the data to send
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::send_DATA(char *buff, uint64_t len){
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_DATA);
    serverMessage.set_data(buff, len);    //insert file block in message

    //send message
    std::string client_message = serverMessage.SerializeAsString();
    s.sendString(client_message);

    //clear the message
    serverMessage.Clear();
}

/**
 * send the MKD message to the server
 *
 * @param e Directory_entry to create
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::send_MKD(const std::string &path, Directory_entry &e){
    //create message
    serverMessage.set_version(protocolVersion);
    serverMessage.set_type(messages::ServerMessage_Type_MKD);
    serverMessage.set_path(path);
    serverMessage.set_lastwritetime(e.getLastWriteTime());

    //compute message
    std::string client_temp = serverMessage.SerializeAsString();

    //send message to server
    s.sendString(client_temp);

    //clear the message Object for future use (it is more efficient to re-use the same object than to create a new one)
    serverMessage.Clear();
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

    TS_Message::print(std::cout, "PROB", address + " (" + username + "@" + mac + ")", path);

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element
        send_SEND();
        return;
    }

    if(!el->second.is_regular_file()) {  //if the element is not a file
        //there is something with the same name which is not a file -> error
        send_ERR(errCode::notAFile);    //send error message with cause
        throw ProtocolManagerException("Probed something which is not a file.", protocolManagerError::client);
        return;
    }

    if(el->second.getHash() != h) {   //if the file hash does not correspond then it means there exists a file which is different
        //we want to overwrite it so we say to the client to send the file
        send_SEND();
        return;
    }

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

    TS_Message::print(std::cout, "STOR", address + " (" + username + "@" + mac + ")", expected.getRelativePath());

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
            do {
                std::string message = s.recvString();
                clientMessage.ParseFromString(message);

                if (clientMessage.version() != protocolVersion) {
                    //send the version message
                    send_VER();

                    //close the file
                    out.close();

                    //delete temporary file
                    std::filesystem::remove(temporaryPath + tmpFileName);

                    //throw exception
                    throw ProtocolManagerException("Client is using a different version",
                                                   protocolManagerError::version);
                }

                if (clientMessage.type() != messages::ClientMessage_Type_DATA) {
                    //close the file
                    out.close();

                    //delete temporary file
                    std::filesystem::remove(temporaryPath + tmpFileName);

                    //no data message with last bool set was encountered before this message, error!
                    send_ERR(errCode::unexpected);    //send error message with cause

                    throw ProtocolManagerException("Unexpected message, DATA transfer was not done.",
                                                   protocolManagerError::client);
                }
                loop = !clientMessage.last();  //is this the last data packet? if yes stop the loop

                std::string data = clientMessage.data();    //get the data from message
                out.write(data.data(), data.size());    //write it to file

                clientMessage.Clear();
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
            send_ERR(errCode::store);    //send error message with cause
            throw ProtocolManagerException("Stored file is different than expected.", protocolManagerError::client);
        }

        //get the parent path
        std::filesystem::path parentPath = std::filesystem::path(expected.getAbsolutePath()).parent_path();

        //check if the parent folder already exists
        create = !std::filesystem::directory_entry(parentPath).exists();

        if(create)  //if the directory does not already exist
            std::filesystem::create_directories(parentPath);    //create all the directories (if they do not already exist) up to the parent path

        //take the lastWriteTime of the destination folder before moving the file,
        //any lastWriteTime modification will be requested by the client so we want to keep the same time
        Directory_entry parent{basePath, parentPath.string()};

        //If we are here then the file was successfully transferred and its copy on the server is as expected
        //so move then it can be moved to the final destination
        std::filesystem::rename(temporaryPath + tmpFileName, expected.getAbsolutePath());

        //reset the parent directory lastWriteTime
        if(parent.getAbsolutePath() != basePath)
            parent.set_time_to_file(parent.getLastWriteTime());

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

        TS_Message::print(std::cout, "DATA", address + " (" + username + "@" + mac + ")", expected.getRelativePath());

        //TODO evaluate if to handle socket exception here
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

    TS_Message::print(std::cout, "DELE", address + " (" + username + "@" + mac + ")", path);

    auto el = elements.find(path);
    if(el == elements.end() || !el->second.exists()) { //if i cannot find the element OR the element does not exist, i don't have to remove it
        //the file did not exist, the result is the same
        send_OK(okCode::notThere);
        return;
    }

    //if it is not a file OR if the file hash does not correspond
    if(!el->second.is_regular_file() || el->second.getHash() != h){
        send_ERR(errCode::remove);    //send error message with cause
        throw ProtocolManagerException("Tried to remove something which is not a file or file hash does not correspond.", protocolManagerError::client);
    }

    //get the parent path
    std::filesystem::path parentPath = std::filesystem::path(el->second.getAbsolutePath()).parent_path();

    //take the lastWriteTime of the destination folder before removing the file,
    //any lastWriteTime modification will be requested by the client so we want to keep the same time
    Directory_entry parent{basePath, parentPath.string()};

    //remove the file
    if(!std::filesystem::remove(el->second.getAbsolutePath()))   //true if the file was removed, false if it couldn't remove it
        throw ProtocolManagerException("Could not remove a file", protocolManagerError::internal);

    //reset the parent directory lastWriteTime
    if(parent.getAbsolutePath() != basePath)
        parent.set_time_to_file(parent.getLastWriteTime());

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

    TS_Message::print(std::cout, "MKD", address + " (" + username + "@" + mac + ")", path);

    auto el = elements.find(path);
    if(el != elements.end() && el->second.exists() && el->second.getLastWriteTime() == lastWriteTime){
        //if I already have the directory in the elements map AND the directory exists in the filesystem AND its lastWriteTime is as expected
        send_OK(okCode::created);   //send OK message
        return; //and return
    }
    //else

    //get the parent path (to then get its lastWriteTime in order to keep it the same after the dir create
    auto parentPath = std::filesystem::path(basePath + path).parent_path();
    auto parExists = std::filesystem::directory_entry(parentPath.string()).exists();
    Directory_entry parent;

    if(parExists)   //if the parent directory exists
        //take the lastWriteTime of the destination folder before creating the directory,
        //any lastWriteTime modification will be requested by the client so we want to keep the same time
        parent = Directory_entry{basePath, parentPath.string()};

    if(!std::filesystem::directory_entry(basePath + path).exists())  //if the directory does not already exist
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

    if(parExists && parent.getAbsolutePath() != basePath)   //if the parent directory already existed before (and it is not the base path)
        //reset the parent directory lastWriteTime
        parent.set_time_to_file(parent.getLastWriteTime());

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

    TS_Message::print(std::cout, "RMD", address + " (" + username + "@" + mac + ")", path);

    auto el = elements.find(path);
    if(el == elements.end()) { //if i cannot find the element -> element does not exist, i don't have to remove it
        //the file did not exist, the result is the same
        send_OK(okCode::notThere);
        return;
    }

    auto dirToRemove = el->second;

    if(!dirToRemove.is_directory()){
        //the element is not a directory! -> error
        send_ERR(errCode::notADir);    //send error message with cause
        throw ProtocolManagerException("Tried to remove something which is not a directory.", protocolManagerError::client);
    }

    auto parentPath = std::filesystem::path(basePath + path).parent_path();
    //take the lastWriteTime of the destination folder before removing the directory,
    //any lastWriteTime modification will be requested by the client so we want to keep the same time
    Directory_entry parent{basePath, parentPath.string()};

    std::filesystem::recursive_directory_iterator iter{dirToRemove.getAbsolutePath()};
    std::vector<Directory_entry> elementsToRemove;
    for(auto i: iter){
        //I save the elements in a vector before removing them in order to be sure to save them all
        elementsToRemove.emplace_back(basePath, i);
    }

    //remove the directory (and all its subdirectories and files) from filesystem
    if(std::filesystem::exists(dirToRemove.getAbsolutePath()))
        if(std::filesystem::remove_all(dirToRemove.getAbsolutePath()) == 0)
            throw ProtocolManagerException("Could not remove an element.", protocolManagerError::internal);

    //then remove each sub element from both the db and elements map
    for(auto i: elementsToRemove){
        //remove element from db
        db->remove(username, mac, i.getRelativePath());

        //remove the element from the elements map
        elements.erase(i.getRelativePath());
    }

    //remove element from db
    db->remove(username, mac, dirToRemove.getRelativePath());

    //remove the element from the elements map
    elements.erase(dirToRemove.getRelativePath());

    //reset the parent directory lastWriteTime
    if(parent.getAbsolutePath() != basePath)
        parent.set_time_to_file(parent.getLastWriteTime());

    //the file has been removed
    send_OK(okCode::removed);
}

//TODO comment
void server::ProtocolManager::retrieveUserData(){
    std::string macAddr = clientMessage.macaddress(); //get the macAddress (if present, otherwise a default "" value is passed)
    bool retrAll = clientMessage.all(); //get the all boolean
    clientMessage.Clear();

    std::map<std::string, Directory_entry> toSend{};    //prepare a map with all the elements to send

    if(retrAll){    //if all is true, meaning that the user requested all its files (independently of the machine mac address they are related to)

        TS_Message::print(std::cout, "RETR", address + " (" + username + "@" + mac + ")", "All files");

        auto macs = db->getAllMacAddresses(username);   //get all the user's mac addresses

        for(std::string m: macs){   //for all the mac addresses

            //compose the directory name (all the elements of a username-mac pair will be put in a specific folder in the client
            //(this folder does not need to be sent since the client will create all missing dirs along the path to a file/directory;
            //so I just need to add the directory name as the first folder in the relative path -> this will be inside the destination
            //folder at the client))
            std::stringstream tmp;
            tmp << "/" << username << "_" << std::regex_replace(m, std::regex(":"), "-");

            std::string tempDirName = tmp.str();

            //create the function to be used for each user's element in the db (for the mac m)
            std::function<void(const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> f;
            f = [this, &toSend, &tempDirName](const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string& hash){
                auto current = Directory_entry(basePath, path, size, type, lastWriteTime, Hash(hash));
                toSend.insert({tempDirName+current.getRelativePath(), current});    //pre-append the relative root (username_mac) to the element relative path
            };

            //perform the function f on all the user elements (and mac m)
            db->forAll(username, m, f);
        }
    }
    else{   //if all is false, macaddrees will contain the specific mac address to use

        if(macAddr.empty()){
            //error
            send_ERR(errCode::retrieve);

            throw ProtocolManagerException("Error in client message", protocolManagerError::client);
        }

        TS_Message::print(std::cout, "RETR", address + " (" + username + "@" + mac + ")", "mac = " + macAddr);

        //compose the directory name (all the elements of a username-mac pair will be put in a specific folder in the client
        //(this folder does not need to be sent since the client will create all missing dirs along the path to a file/directory;
        //so I just need to add the directory name as the first folder in the relative path -> this will be inside the destination
        //folder at the client))
        std::stringstream tmp;
        tmp << "/" << username << "_" << std::regex_replace(macAddr, std::regex(":"), "-");

        std::string tempDirName = tmp.str();

        //create the function to be used for each user's element in the db (for the mac m)
        std::function<void(const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> f;
        f = [this, &toSend, &tempDirName](const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string& hash){
            auto current = Directory_entry(basePath, path, size, type, lastWriteTime, Hash(hash));
            toSend.insert({tempDirName+current.getRelativePath(), current});    //pre-append the relative root (username_mac) to the element relative path
        };

        //perform the function f on all the user elements (and mac m)
        db->forAll(username, macAddr, f);
    }

    //here in toSend I will have all the elements to send to the client
    for(auto pair: toSend){
        if(pair.second.is_directory()){ //if it is a directory then send MKD
            TS_Message::print(std::cout, "RETR-MKD", address + " (" + username + "@" + mac + ")", pair.first);
            send_MKD(pair.first, pair.second);
        }
        else if(pair.second.is_regular_file()) {  //if it is a file then send STOR and 1 or more DATA
            TS_Message::print(std::cout, "RETR-STOR", address + " (" + username + "@" + mac + ")", pair.first);
            send_STOR(pair.first, pair.second);
            sendFile(pair.second, macAddr);
        }
        //else the element is not supported so just skip it (there should never be any not supported element)
    }

    send_OK(okCode::retrieved);
}

//TODO comment
void server::ProtocolManager::sendFile(Directory_entry &element, std::string &macAddr) {
    int64_t max_data_chunk_size = 1024; //TODO max_data_chunk_size to config
    //initialize variables
    std::ifstream file;
    char buff[max_data_chunk_size];

    if(!std::filesystem::exists(element.getAbsolutePath())) {
        //if the file to transfer is not present anymore in the filesystem
        //then it means that the file was deleted --> I can't send it anymore
        //so remove it from the db and just go to the next element (so return)
        db->remove(username, macAddr, element.getRelativePath());
        return;
    }

    //if the file to transfer hash is different from the one of the file present in the filesystem
    Directory_entry effective{basePath, element.getAbsolutePath()}; //I do this because the element I got as argument was retrieved from db, instead this one is retrieved from filesystem
    if(effective.getHash() != element.getHash()){
        //then it means that the file was modified --> Don't send it
        //so remove it from the db and just go to the next element (so return)
        db->remove(username, macAddr, element.getRelativePath());
        return;
    }

    //open input file
    file.open(element.getAbsolutePath(), std::ios::in | std::ios::binary);

    if(file.is_open()){
        int64_t totRead = 0;
        //TS_Message::print(std::cout, "SENDING", address + " (" + username + "@" + mac + ")", "Sending: " + element.getRelativePath());

        while(file.read(buff, max_data_chunk_size)) { //read file in max_data_chunk_size-wide blocks
            totRead += file.gcount();
            send_DATA(buff, file.gcount()); //send the block
        }

        serverMessage.set_last(true);   //mark the last data block

        totRead += file.gcount();
        send_DATA(buff, file.gcount()); //send the block

        //close the input file
        file.close();
    }
    else
        throw ProtocolManagerException("Could not open file", protocolManagerError::internal);
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
server::ProtocolManager::ProtocolManager(Socket &s, std::string address, int ver) :
    s(s),
    address(std::move(address)),
    protocolVersion(ver),
    recovered(false){

    auto config = Config::getInstance(CONFIG_FILE_PATH);
    basePath = config->getServerBasePath();
    temporaryPath = config->getTempPath();
    tempNameSize = config->getTmpFileNameSize();

    password_db = PWD_Database::getInstance(config->getPasswordDatabasePath());
    db = Database::getInstance(config->getServerDatabasePath());
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

    TS_Message::print(std::cout, "EVENT", address, "authenticated as " + username  + "@" + mac);
};

/**
 * function to recover all the elements in the server db and populate the elements map (used at protocol manager startup)
 *
 * @throw DatabaseException from the db->forall function
 *
 * @author Michele Crepaldi s269551
 */
void server::ProtocolManager::recoverFromDB() {
    if(recovered)   //if I already did the recovery just return
        return;

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
        TS_Message::print(std::cerr, "WARNING", el.getRelativePath() + " in " + basePath, "was modified offline!");
        db->update(username, mac, el);
    }

    //delete all the deleted elements from db
    for(auto el: toDelete){
        TS_Message::print(std::cerr, "WARNING", el.getRelativePath() + " in " + basePath, "was removed offline!");
        db->remove(username, mac, el.getRelativePath());
    }

    recovered = true;   //set this bool meaning the recovery has been completed
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

                //TODO evaluate how to move this in another place in order not to repeat it all the time
                recoverFromDB();    //will be done only the first time

                probe();    //probe the elements list for the element in client message

                break;
            case messages::ClientMessage_Type_STOR: {

                recoverFromDB();    //will be done only the first time
                storeFile();    //store the file in the server filesystem

                break;
            }
            case messages::ClientMessage_Type_DELE:

                recoverFromDB();    //will be done only the first time
                removeFile();

                break;
            case messages::ClientMessage_Type_MKD:

                recoverFromDB();    //will be done only the first time
                makeDir();

                break;
            case messages::ClientMessage_Type_RMD:

                recoverFromDB();    //will be done only the first time
                removeDir();

                break;

            case messages::ClientMessage_Type_RETR:

                retrieveUserData();

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
    catch (server::ConfigException &e){
        throw;  //rethrow exception (I don't want the general std::exception catch to catch it)
    }
    catch (std::exception &e) {
        //internal error
        send_ERR(errCode::exception);    //send error message with cause

        throw ProtocolManagerException(e.what(),server::protocolManagerError::internal);
    }
}