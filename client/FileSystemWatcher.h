//
// Created by michele on 25/07/2020.
//

#ifndef PDS_BACKUP_FILESYSTEMWATCHER_H
#define PDS_BACKUP_FILESYSTEMWATCHER_H

#include <filesystem>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <functional>
#include <iostream>
#include <utility>
#include "Directory_entry.h"

using namespace std::chrono_literals;

/**
 * define all possible filesystem modifications to be watched
 *
 * @author Michele Crepaldi s269551
 */
enum class FileSystemStatus {created, modified, deleted};

/**
 * class to use to watch the file system for changes
 *
 * @author Michele Crepaldi s269551
 */
class FileSystemWatcher {
public:
    // path of the root directory to watch
    std::string path_to_watch;
    // Time interval at which we check the base folder for changes
    std::chrono::duration<int, std::milli> delay;

    /**
     * constructor function for this class:
     * Keep a record of files from the base directory and their last modification time
     *
     * @author Michele Crepaldi s269551
     */
    FileSystemWatcher(std::string  path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{std::move(path_to_watch)}, delay{delay} {
    }

    /**
     * Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
     *
     * @param action to be performed
     *
     * @author Michele Crepaldi s269551
     */
    void start(const std::function<bool (Directory_entry&, FileSystemStatus)> &action) {
        while(running_) {
            // Wait for "delay" milliseconds
            std::this_thread::sleep_for(delay);

            // check if a file/directory was deleted
            auto it = paths_.begin();
            while (it != paths_.end()) {
                if (!std::filesystem::exists(it->first)) {
                    if(action(it->second, FileSystemStatus::deleted)) //if the action was successful then erase the element from paths_; otherwise this element will be removed later
                        it = paths_.erase(it);
                }
                else{
                    it++;
                }
            }
            // Check if a file/directory was created or modified
            for(const auto& file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
                auto current_file = Directory_entry(file);

                if(!contains(file.path().string())) { //file creation
                    if(action(current_file, FileSystemStatus::created)) //if the action was successful then add the element to paths_; otherwise this element will be added later
                        paths_[current_file.getPath()] = current_file;

                } else if(paths_[current_file.getPath()].getLastWriteTime() != current_file.getLastWriteTime()) { //file modification
                    if(action(current_file, FileSystemStatus::modified)) //if the action was successful then overwrite the element to paths_; otherwise this element will be overwritten later
                        paths_[current_file.getPath()] = current_file;
                }
            }
        }
    }

    /**
     * stops the FileSystemWatcher
     * @return nothing
     *
     * @author Michele Crepaldi s269551
     */
    void stop(){
        running_ = false;
    }

private:
    std::unordered_map<std::string, Directory_entry> paths_;
    bool running_ = true;

    /**
     * Check if "paths_" contains a given key
     *
     * @param key
     * @return whether the paths_ map contains key in the set of keys
     *
     * @author Michele Crepaldi s269551
     */
    bool contains(const std::string &key) {
        auto el = paths_.find(key);
        return el != paths_.end();
    }
};


#endif //PDS_BACKUP_FILESYSTEMWATCHER_H
