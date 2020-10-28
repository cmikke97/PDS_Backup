//
// Created by michele on 21/10/2020.
//

#include <fstream>
#include "ProtocolManager.h"

//TODO i will get these from config
#define PASSWORD_DATABASE_PATH "./serverfiles/passwordDB.sqlite"
#define DATABASE_PATH "./serverfiles/serverDB.sqlite"
#define VERSION 1

#define MAXBUFFSIZE 1024

server::ProtocolManager::ProtocolManager(Socket &s, int ver, const std::string &basePath) : s(s), protocolVersion(ver), basePath(basePath) {
    //TODO get from config
    password_db = PWD_Database::getInstance(PASSWORD_DATABASE_PATH);
    db = Database::getInstance(DATABASE_PATH);
};

void server::ProtocolManager::errorHandler(protocolManagerError code){
    serverMessage.set_version(protocolVersion); //TODO properly get version
    serverMessage.set_type(messages::ServerMessage_Type_ERR);
    serverMessage.set_code(static_cast<int>(code));  //TODO error code

    //send error message
    std::string tmp = serverMessage.SerializeAsString();
    serverMessage.Clear();
    s.sendString(tmp);

    //throw exception
    throw ProtocolManagerException("Message type not expected", code); //TODO error code
}

void server::ProtocolManager::authenticate() {
    std::string message = s.recvString();
    clientMessage.ParseFromString(message);

    //check version
    if(protocolVersion != clientMessage.version())
        std::cerr << "different protocol version!" << std::endl; //TODO decide if to keep this and in case yes handle it

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
            errorHandler(protocolManagerError::auth); //TODO error code
        }
    }
    else{
        //error, message not expected
        errorHandler(protocolManagerError::unknown);    //TODO error code
    }

    std::stringstream tmp;
    tmp << basePath << "/" << username << "_" << "mac" << "/";
    basePath = tmp.str();

    clientMessage.Clear();
};

void server::ProtocolManager::recoverFromDB() {
    std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> f;
    f = [this](const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string& hash){
        auto element = Directory_entry(basePath, path, size, type, lastWriteTime, Hash(hash));
        elements.insert({path, element}); //TODO check base path
    };
    db->forAll(username, mac, f);
}

bool probe(messages::ClientMessage &c, std::unordered_map<std::string, Directory_entry> &els){
    auto el = els.find(c.path());
    if(el == els.end()) //if i cannot find the element
        return false;

    if(!el->second.is_regular_file())   //if the element is not a file
        return false;

    Hash h = Hash(c.hash());
    if(el->second.getHash() != h)   //if the file hash does not correspond
        return false;

    if(el->second.getSize() != c.filesize())    //if the filesize does not correspond
        return false;

    if(el->second.getLastWriteTime() != c.lastwritetime())  //if the last write time does not correspond
        return false;

    return true;
}

bool storeFile(Socket &s, messages::ClientMessage &cm, const std::string &basePath){
    const std::string& path = cm.path();
    uintmax_t size = cm.filesize();
    std::string lastwriteTime = cm.lastwritetime();
    Hash h{cm.hash()};
    cm.Clear();

    HashMaker hm;

    //TODO decide if to permit overwrite
    std::ofstream out;
    out.open(basePath + path, std::ofstream::out | std::ofstream::binary);
    if(out.is_open()){
        bool loop = true;
        do{
            std::string message = s.recvString();
            cm.ParseFromString(message);

            if(cm.version() != protocol_version){
                //TODO manage this error
            }

            if(cm.type() != messages::ClientMessage_Type_DATA){
                //TODO error, throw exception
            }

            loop = !cm.last();  //is this the last data packet? if yes stop the loop

            //TODO it may be better to write the file in a temporary folder, check it at the end and then move
            // it in the destination folder if it is ok
            std::string data = cm.data();
            out.write(data.data(), data.size());
            hm.update(data.data(), data.size());    //update the calculated hash

            cm.Clear();
        }
        while(loop);

        //TODO change last write time for the file
        std::filesystem::directory_entry element(basePath + path);
        //element.last_write_time();

        //if the written file has a different size from what expected
        if(element.file_size() != size)
            return false;

        Hash hash = hm.get();
        if(hash != h)   //if the calculated hash is different from what expected
            return false;

    }

    return true;
}

bool removeFile(messages::ClientMessage &c, std::unordered_map<std::string, Directory_entry> &els){
    std::string path = c.path();
    Hash h{c.hash()};

    auto el = els.find(c.path());
    if(el == els.end()) //if i cannot find the element -> element does not exist, i don't have to remove it
        return true;

    if(!el->second.is_regular_file()) //if it is not a file
        return false;

    if(el->second.getHash() != h)   //if the file hash does not correspond
        return false;

    //remove the file
    std::filesystem::remove(el->second.getAbsolutePath());   //true if the file was removed, false if it did not exist
    return true;
}

void server::ProtocolManager::receive(){
    //get client request
    std::string message = s.recvString();

    //parse client request
    clientMessage.ParseFromString(message);

    //check version
    if(protocolVersion != clientMessage.version())
        std::cerr << "different protocol version!" << std::endl; //TODO decide if to keep this and in case yes handle it

    //TODO go on
    std::string tmp;
    switch(clientMessage.type()) {
        case messages::ClientMessage_Type_PROB:
            serverMessage.set_version(protocolVersion);

            if (probe(clientMessage, elements)) {
                //the file has been found
                serverMessage.set_type(messages::ServerMessage_Type_OK);
                serverMessage.set_code(0);
            } else {
                //the file was not there
                serverMessage.set_type(messages::ServerMessage_Type_SEND);
                serverMessage.set_path(clientMessage.path());
                serverMessage.set_filesize(clientMessage.filesize());
                serverMessage.set_lastwritetime(clientMessage.lastwritetime());
                serverMessage.set_path(clientMessage.path());
                serverMessage.set_hash(clientMessage.hash());
            }

            tmp = serverMessage.SerializeAsString();
            s.sendString(tmp);
            serverMessage.Clear();

            break;
        case messages::ClientMessage_Type_STOR:
        {
            serverMessage.set_version(protocolVersion);
            Directory_entry element{basePath, clientMessage.path(), clientMessage.filesize(), "file", clientMessage.lastwritetime(), Hash{clientMessage.hash()}};

            if(storeFile(s, clientMessage, basePath)){
                //the file has been created
                serverMessage.set_type(messages::ServerMessage_Type_OK);
                serverMessage.set_code(0);

                //add to the queue
                elements.emplace(element.getRelativePath(), element);

                //add to db
                db->insert(username, mac, element);
            }
            else{
                //some error occured
                serverMessage.set_type(messages::ServerMessage_Type_ERR);
                serverMessage.set_code(100);    //TODO error code
            }
            tmp = serverMessage.SerializeAsString();
            s.sendString(tmp);
            serverMessage.Clear();
            //TODO get all data messages from client and create file on filesystem, db and map
            break;
        }
        case messages::ClientMessage_Type_DELE:
            serverMessage.set_version(protocolVersion);

            if(removeFile(clientMessage, elements)){
                //the file has been removed
                serverMessage.set_type(messages::ServerMessage_Type_OK);
                serverMessage.set_code(0);
            }
            else{
                //some error occured
                serverMessage.set_type(messages::ServerMessage_Type_ERR);
                serverMessage.set_code(100);    //TODO error code
            }

            tmp = serverMessage.SerializeAsString();
            s.sendString(tmp);
            serverMessage.Clear();
            //TODO delete element from elements map, from filesystem and from db (do nothing if the element does
            // not exist
            break;
        case messages::ClientMessage_Type_MKD:
            //TODO add element in elements map (if not already there), create dir in filesystem (modify last write
            // time if already there), add element to db
            break;
        case messages::ClientMessage_Type_RMD:
            //TODO remove directory from db, filesystem and map
            break;
        case messages::ClientMessage_Type_QUIT:
            //TODO close connection
            break;
        default:
            throw ProtocolManagerException("Unknown message type", protocolManagerError::unknown);
    }

    clientMessage.Clear();
}