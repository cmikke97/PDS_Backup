//
// Created by michele on 25/07/2020.
//

#include "FileSystemWatcher.h"

/**
     * constructor function for this class:
     * Keep a record of files from the base directory and their last modification time
     *
     * @author Michele Crepaldi s269551
     */
FileSystemWatcher::FileSystemWatcher(std::string  path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{std::move(path_to_watch)}, delay{delay} {
    //set directory entry class base directory to the path to watch
    Directory_entry::setBaseDir(path_to_watch);

    //TODO link to database and retrieve previous file state
}

/**
 * Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
 *
 * @param action to be performed
 *
 * @author Michele Crepaldi s269551
 */
void FileSystemWatcher::start(const std::function<bool (Directory_entry&, FileSystemStatus)> &action, std::atomic<bool> &stop) {
    while(!stop.load()) {
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
        // Check if a file/directory was created
        for(const auto& file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
            auto current_file = Directory_entry(file);

            if(current_file.getType() == Directory_entry_TYPE::notAType) //if it is not a file nor a directory don't do anything and go on
                continue;

            if(!contains(file.path().string())) { //file creation
                if(action(current_file, FileSystemStatus::created)) //if the action was successful then add the element to paths_; otherwise this element will be added later
                    paths_[current_file.getAbsolutePath()] = current_file;
            }
        }
    }
}

/**
 * Check if "paths_" contains a given key
 *
 * @param key
 * @return whether the paths_ map contains key in the set of keys
 *
 * @author Michele Crepaldi s269551
 */
bool FileSystemWatcher::contains(const std::string &key) {
    auto el = paths_.find(key);
    return el != paths_.end();
}