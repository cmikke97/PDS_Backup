//
// Created by Michele Crepaldi s269551 on 27/07/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#ifndef CLIENT_EVENT_H
#define CLIENT_EVENT_H

#include "../myLibraries/Directory_entry.h"
#include "FileSystemWatcher.h"


/**
 * Event class. Used to represent a signle filesystem event
 *
 * @author Michele Crepaldi s269551
 */
class Event {
public:
    Event() = default;  //default empty constructor
    Event(const Event &) = default; //default copy constructor
    Event& operator=(const Event &) = default;  //default copy assignment
    Event(Event &&) = default;  //default move constructor
    Event& operator=(Event &&) = default;   //default move assignment

    Event(Directory_entry&  element, FileSystemStatus type);  //Event constructor


    //getters

    Directory_entry& getElement();
    FileSystemStatus getType();

private:
    Directory_entry _element{};   //directory entry element this event refers to
    FileSystemStatus _type{};     //type of modification
};


#endif //CLIENT_EVENT_H
