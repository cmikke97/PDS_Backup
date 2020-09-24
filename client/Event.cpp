//
// Created by michele on 27/07/2020.
//

#include "Event.h"

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

Directory_entry & Event::getElement() {
    return element;
}

FileSystemStatus Event::getStatus() {
    return status;
}
