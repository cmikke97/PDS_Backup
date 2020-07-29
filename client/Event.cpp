//
// Created by michele on 27/07/2020.
//

#include "Event.h"

#include <utility>
#include <cstring>

/**
 * constructor of Event Object
 *
 * @param element directory entry associated to this event
 * @param status event modification
 *
 * @author Michele Crepaldi s269551
 */
Event::Event(Directory_entry& element, FileSystemStatus status) : element(element), status(status) {
}

/**
 * default constructor
 *
 * @author Michele Crepaldi s269551
 */
Event::Event() : element(), status() {
}

/**
 * generate probe header message for this event
 *
 * @return message header
 *
 * @author Michele Crepaldi s269551
 */
Message Event::getProbe() {
    Hash h;

    if(status == FileSystemStatus::modified)
        h = element.getPrevHash();
    else
        h = element.getHash();

    auto hash_pair = h.getValue();

    unsigned long header_length = sizeof(unsigned char)*(
            sizeof(unsigned int)+   //version
            sizeof(messageType)+    //type
            sizeof(unsigned long)+  //hash length
            hash_pair.second);      //hash

    m = Message{header_length + sizeof(unsigned long)};
    m.append(header_length);
    m.append(version);
    m.append(messageType::PH);
    m.append(hash_pair.second);
    m.append(reinterpret_cast<const char *>(hash_pair.first), hash_pair.second);
    return m;
}

/**
 * generate create header message for this event
 *
 * @return message header
 *
 * @author Michele Crepaldi s269551
 */
Message Event::getCreateHeader() {
    auto hash_pair = element.getHash().getValue();

    messageType type = element.getType()==Directory_entry_TYPE::file? messageType::FC : messageType::DC;

    //calculate header length
    unsigned long header_length;
    if(element.getType()==Directory_entry_TYPE::file) { //is file
        header_length = sizeof(unsigned char) * (
                sizeof(unsigned int) +   //version
                sizeof(messageType) +    //type
                sizeof(size_t) +         //path length
                element.getPath().length() + //path
                sizeof(uintmax_t) +      //size
                sizeof(size_t) +         //last write time length
                element.getLastWriteTime().length() +    //last write time
                sizeof(unsigned long) +  //hash length
                hash_pair.second);      //hash
    }
    else { //is directory
        header_length = sizeof(unsigned char) * (
                sizeof(unsigned int) +   //version
                sizeof(messageType) +    //type
                sizeof(size_t) +         //path length
                element.getPath().length() + //path
                sizeof(unsigned long) +  //hash length
                hash_pair.second);      //hash
    }

    //initialize message
    m = Message{header_length + sizeof(unsigned long)};

    //add header length
    m.append(header_length);
    //add message version
    m.append(version);
    //add message type
    m.append(type);

    //add file path
    m.append(element.getPath().length());
    m.append(element.getPath());

    if(element.getType()==Directory_entry_TYPE::file){
        //add file size
        //m.append(sizeof(uintmax_t));
        m.append(element.getSize());

        //add last write time
        m.append(element.getLastWriteTime().length());
        m.append(element.getLastWriteTime());
    }

    //add hash
    m.append(hash_pair.second);
    m.append(reinterpret_cast<const char *>(hash_pair.first), hash_pair.second);
    return m;
}

/**
 * generate edit header message for this event
 *
 * @return message header
 *
 * @author Michele Crepaldi s269551
 */
Message Event::getEditHeader() {
    auto hash_pair = element.getHash().getValue();
    auto prev_hash_pair = element.getPrevHash().getValue();

    messageType type = element.getType()==Directory_entry_TYPE::file? messageType::FE : messageType::DE;

    //calculate header length
    unsigned long header_length;
    if(element.getType()==Directory_entry_TYPE::file) { //is file
        header_length = sizeof(unsigned char) * (
                sizeof(unsigned int) +  //version
                sizeof(messageType) +   //type
                sizeof(size_t) +        //path length
                element.getPath().length() +    //path
                sizeof(uintmax_t) +     //size
                sizeof(size_t) +        //last write time length
                element.getLastWriteTime().length() +   //last write time
                sizeof(unsigned long) +     //previous hash length
                prev_hash_pair.second +     //previous hash
                sizeof(unsigned long) +     //new hash length
                hash_pair.second);          //new hash
    }
    else { //is directory
        header_length = sizeof(unsigned char) * (
                sizeof(unsigned int) +  //version
                sizeof(messageType) +   //type
                sizeof(size_t) +        //path length
                element.getPath().length() +    //path
                sizeof(unsigned long) +     //previous hash length
                prev_hash_pair.second +     //previous hash
                sizeof(unsigned long) +     //new hash length
                hash_pair.second);          //new hash
    }

    //initialize message
    m = Message{header_length + sizeof(unsigned long)};

    //add header length
    m.append(header_length);
    //add message version
    m.append(version);
    //add message type
    m.append(type);

    //add file path
    m.append(element.getPath().length());
    m.append(element.getPath());

    if(element.getType()==Directory_entry_TYPE::file) {
        //add file size
        //m.append(sizeof(uintmax_t));
        m.append(element.getSize());

        //add last write time
        m.append(element.getLastWriteTime().length());
        m.append(element.getLastWriteTime());
    }

    //add previous hash
    m.append(prev_hash_pair.second);
    m.append(reinterpret_cast<const char *>(prev_hash_pair.first), prev_hash_pair.second);

    //add hash
    m.append(hash_pair.second);
    m.append(reinterpret_cast<const char *>(hash_pair.first), hash_pair.second);
    return m;
}

/**
 * generate delete header message for this event
 *
 * @return message header
 *
 * @author Michele Crepaldi s269551
 */
Message Event::getDeleteHeader() {
    auto hash_pair = element.getHash().getValue();

    messageType type = element.getType()==Directory_entry_TYPE::file? messageType::FD : messageType::DD;

    //calculate header length
    unsigned long header_length = sizeof(unsigned char)*(
            sizeof(unsigned int)+   //version
            sizeof(messageType)+    //type
            sizeof(unsigned long)+  //hash length
            hash_pair.second);      //hash

    //initialize message
    m = Message{header_length + sizeof(unsigned long)};

    //add header length
    m.append(header_length);
    //add message version
    m.append(version);
    //add message type
    m.append(type);

    //add hash
    m.append(hash_pair.second);
    m.append(reinterpret_cast<const char *>(hash_pair.first), hash_pair.second);
    return m;
}

/**
 * generate Get Salt header message
 *
 * @param username
 * @return message
 *
 * @author Michele Crepaldi s269551
 */
Message Event::getSaltHeader(const std::string& username) {
    //calculate header length
    unsigned long header_length = sizeof(unsigned char)*(
            sizeof(unsigned int)+   //version
            sizeof(messageType)+    //type
            sizeof(size_t)+         //username length
            username.length());     //username

    //initialize message
    m = Message{header_length + sizeof(unsigned long)};

    //add header length
    m.append(header_length);
    //add message version
    m.append(version);
    //add message type
    m.append(messageType::GS);

    //add username
    m.append(username.length());
    m.append(username);
    return m;
}

/**
 * generate Auth User header message
 *
 * @param username
 * @param password of the user
 * @param salt associated to the user
 * @return message
 *
 * @author Michele Crepaldi s269551
 */
Message Event::getAuthHeader(const std::string& username, const std::string& password, char* salt) {
    std::stringstream temp;
    std::string salt_s{(salt)};
    temp << password << salt;
    Hash password_hash = Hash{reinterpret_cast<const unsigned char *>(temp.str().c_str()), temp.str().length()};
    auto hash_pair = password_hash.getValue();

    //calculate header length
    unsigned long header_length = sizeof(unsigned char)*(
            sizeof(unsigned int)+   //version
            sizeof(messageType)+    //type
            sizeof(size_t)+         //username length
            username.length()+      //username
            sizeof(unsigned long)+  //password (salted) hash length
            hash_pair.second);      //password (salted) hash

    //initialize message
    m = Message{header_length + sizeof(unsigned long)};

    //add header length
    m.append(header_length);
    //add message version
    m.append(version);
    //add message type
    m.append(messageType::AU);

    //add username
    m.append(username.length());
    m.append(username);

    //add password hash
    m.append(hash_pair.second);
    m.append(reinterpret_cast<const char *>(hash_pair.first), hash_pair.second);
    return m;
}

/**
 * get an element value from a char (byte) array
 *
 * @tparam T type of the element to get
 * @param element where to put the result
 * @param array from where to get the desired element
 *
 * @author Michele Crepaldi s269551
 */
template<typename T>
void getElementFromCharArray(T& element, char **array) {
    memcpy(&element,*array,sizeof(T));
    *array+=sizeof(T);
}

/**
 * get the type of the response message
 *
 * @param response message
 * @return response type
 *
 * @author Michele Crepaldi s269551
 */
messageType Event::getType(char *response) {
    unsigned int resVersion;
    getElementFromCharArray(resVersion, &response);

    if(resVersion != version)
        return messageType::CV;

    messageType resType;
    getElementFromCharArray(resType, &response);
    return resType;
}

/**
 * get the salt from the US response message
 *
 * @param response message
 * @param salt of the user
 *
 * @author Michele Crepaldi s269551
 */
void Event::getUserSalt(char *response, char *salt) {
    unsigned int resVersion;
    getElementFromCharArray(resVersion, &response);
    messageType resType;
    getElementFromCharArray(resType, &response);

    if(resType == messageType::US){
        unsigned long salt_len;
        getElementFromCharArray(salt_len, &response);
        memcpy(salt, response, salt_len* sizeof(unsigned char));
    }
}

/**
 * get error code from ER response message
 *
 * @param response message
 * @return error code
 *
 * @author Michele Crepaldi s269551
 */
int Event::getErrorCode(char *response) {
    unsigned int resVersion;
    getElementFromCharArray(resVersion, &response);
    messageType resType;
    getElementFromCharArray(resType, &response);

    if(resType == messageType::ER){
        int errorCode;
        getElementFromCharArray(errorCode, &response);
        return errorCode;
    }
    return -1;
}

Directory_entry &Event::getElement() {
    return element;
}

FileSystemStatus Event::getStatus() {
    return status;
}
