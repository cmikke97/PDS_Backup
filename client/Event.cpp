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
 * function to get the element from this event
 *
 * @return directory entry element
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry & Event::getElement() {
    return element;
}

/**
 * function to get the status from this event
 *
 * @return status of this event
 *
 * @author Michele Crepaldi s269551
 */
FileSystemStatus Event::getStatus() {
    return status;
}
