//
// Created by michele on 27/07/2020.
//

#ifndef CLIENT_EVENT_H
#define CLIENT_EVENT_H

#include "Directory_entry.h"
#include "FileSystemWatcher.h"


class Event {
    Directory_entry element;
    FileSystemStatus status;

public:
    Event(Directory_entry&  element, FileSystemStatus status);

    //TODO implement message composers

    /*
    if(element_to_watch.is_regular_file()){
        switch(status) {
            case FileSystemStatus::created:
                std::cout << "File created: " << element_to_watch.getPath() << std::endl;
                break;
            case FileSystemStatus::modified:
                std::cout << "File modified: " << element_to_watch.getPath() << std::endl;
                break;
            case FileSystemStatus::deleted:
                std::cout << "File deleted: " << element_to_watch.getPath() << std::endl;
                break;
            default:
                std::cout << "Error! Unknown file status." << std::endl;
        }
    }
    else if(element_to_watch.is_directory()){
        switch(status) {
            case FileSystemStatus::created:
                std::cout << "Directory created: " << element_to_watch.getPath() << std::endl;
                break;
            case FileSystemStatus::modified:
                std::cout << "Directory modified: " << element_to_watch.getPath() << std::endl;
                break;
            case FileSystemStatus::deleted:
                std::cout << "Directory deleted: " << element_to_watch.getPath() << std::endl;
                break;
            default:
                std::cout << "Error! Unknown file status." << std::endl;
        }
    }
    else{
        std::cout << "change to an unsupported type." << std::endl;
    }
    */

};


#endif //CLIENT_EVENT_H
