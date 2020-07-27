//
// Created by michele on 27/07/2020.
//

#include "Event.h"

#include <utility>

Event::Event(Directory_entry& element, FileSystemStatus status) : element(element), status(status) {
}

Event::Event() : element(), status() {
}
