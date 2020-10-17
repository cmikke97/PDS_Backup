//
// Created by michele on 27/07/2020.
//

#ifndef CLIENT_EVENT_H
#define CLIENT_EVENT_H

#include "../myLibraries/Directory_entry.h"
#include "FileSystemWatcher.h"

/**
 * Event class
 *
 * @author Michele Crepaldi s269551
 */
class Event {
    Directory_entry element;
    FileSystemStatus status;

public:
    Event();
    Event(Directory_entry&  element, FileSystemStatus status);
    Directory_entry& getElement();
    FileSystemStatus getStatus();
};


#endif //CLIENT_EVENT_H
