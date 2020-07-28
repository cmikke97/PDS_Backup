//
// Created by michele on 27/07/2020.
//

#ifndef CLIENT_EVENT_H
#define CLIENT_EVENT_H

#include "Directory_entry.h"
#include "FileSystemWatcher.h"
#include "Message.h"

const unsigned int version = 1;

/**
 * all possible message types
 *
 * @author Michele Crepaldi s269551
 */
enum class messageType{PH,FC,FE,FD,DC,DE,DD,GS,AU,US,CV,ER,OK,NO};

/**
 * Event class
 *
 * @author Michele Crepaldi s269551
 */
class Event {
    Directory_entry element;
    FileSystemStatus status;
    Message m;

public:
    Event();
    Event(Directory_entry&  element, FileSystemStatus status);
    Directory_entry& getElement();
    FileSystemStatus getStatus();
    Message getProbe();
    Message getCreateHeader();
    Message getEditHeader();
    Message getDeleteHeader();
    Message getSalt(const std::string& username);
    Message getAuthHeader(const std::string& username, const std::string& password, char* salt);

    static messageType getType(char* response);
    static void getUserSalt(char* response, char* salt);
    static int getErrorCode(char* response);
    //unsigned int getVersion(char* response);
};


#endif //CLIENT_EVENT_H
