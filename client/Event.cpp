//
// Created by Michele Crepaldi s269551 on 27/07/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#include "Event.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Event class methods
 */

/**
 * Event class empty constructor
 *
 * @author Michele Crepaldi s269551
 */
Event::Event() : _element(), _type(FileSystemStatus::notAStatus) {
}

/**
 * Event class constructor
 *
 * @param element directory entry associated to this event
 * @param type type of modification
 *
 * @author Michele Crepaldi s269551
 */
Event::Event(Directory_entry& element, FileSystemStatus type) : _element(element), _type(type) {
}

/**
 * function to get the element from this event
 *
 * @return directory entry element
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry& Event::getElement() {
    return _element;
}

/**
 * method used to get the type of modification from this event
 *
 * @return type of modification
 *
 * @author Michele Crepaldi s269551
 */
FileSystemStatus Event::getType() {
    return _type;
}
