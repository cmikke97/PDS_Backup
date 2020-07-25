//
// Created by michele on 25/07/2020.
//

#include <chrono>
#include <string>
#include <iostream>
#include "FileSystemWatcher.h"
#include "Directory_entry.h"

/**
 * the client main function
 *
 * @param argc command argument number
 * @param argv command arguments
 * @return exit value
 *
 * @author Michele Crepaldi s269551
 */
int main(int argc, char **argv) {

    if(argc != 3){
        std::cerr << "Error: format is [" << argv[0] << "] [directory to watch] [server ip address]" << std::endl;
        exit(1);
    }

    // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
    FileSystemWatcher fw{argv[1], std::chrono::milliseconds(5000)};

    // Start monitoring a folder for changes and (in case of changes)
    // run a user provided lambda function
    fw.start([](Directory_entry element_to_watch, FileSystemStatus status) -> void {

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
    });
}