//
// Created by michele on 25/07/2020.
//

#include "FileSystemWatcher.h"

//TODO check

/**
 * constructor function for this class:
 * Keep a record of files from the base directory and their last modification time
 *
 * @param path_to_watch path this FileSystemWatcher has to watch
 * @param delay amount of time to wait between polls
 *
 * @author Michele Crepaldi s269551
 */
FileSystemWatcher::FileSystemWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{std::move(path_to_watch)}, delay{delay} {
}

/**
 * Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
 *
 * @param action to be performed
 * @param stop atomic boolean to make this FileSystemWatcher to stop (set to true by the communication thread in case of exceptions)
 *
 * @author Michele Crepaldi s269551
 */
void FileSystemWatcher::start(const std::function<bool (Directory_entry&, FileSystemStatus)> &action, std::atomic<bool> &stop) {

    while(!stop.load()) {   //loop until I am told to stop
        //Wait for "delay" milliseconds
        std::this_thread::sleep_for(delay);

        //check if a file/directory was deleted
        //TODO check if the files belong to the path to watch set
        auto it = paths_.begin();
        while (it != paths_.end()) {
            if (!std::filesystem::exists(it->first)) {
                //std::filesystem::path parent = std::filesystem::path(it->first).parent_path();  //get the parent directory of this element

                if(action(it->second, FileSystemStatus::deleted)) //if the action was successful then erase the element from paths_; otherwise this element will be removed later
                    it = paths_.erase(it);

                /*
                if(std::filesystem::exists(parent)){    //if the parent still exists then when at the server the lastWriteTime will be modified (an element inside the folder will be removed)
                    auto old = paths_.find(parent.string());
                    if(old != paths_.end())
                        action(old->second, FileSystemStatus::modified);
                }*/
            }
            else{
                it++;
            }
        }
        //Check if a file/directory was created
        //TODO check if the path_to_watch exists
        for(const auto& file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
            auto current = Directory_entry(path_to_watch, file);

            if(!current.is_directory() && !current.is_regular_file()) //if it is not a file nor a directory don't do anything and go on
                continue;

            auto old = paths_.find(file.path().string());
            if(old == paths_.end()) { //file creation
                if(action(current, FileSystemStatus::created)) //if the action was successful then add the element to paths_; otherwise this element will be added later
                    paths_[current.getAbsolutePath()] = std::move(current);
            }else {
                auto el = old->second;
                if (el.getLastWriteTime() != current.getLastWriteTime() || el.getType() != current.getType() ||
                        el.getSize() != current.getSize() || el.getHash() != current.getHash()) { //file modify
                    if (action(current, FileSystemStatus::modified))
                        old->second = std::move(current);
                }
            }
        }
    }
}

/**
 * function used by this class to retrieve previously save data (about the entries) from the db
 *
 * @param db db to retrieve data from
 * @param action action to perform for each row of the db
 *
 * @throw runtime exception in case of db errors
 *
 * @author Michele Crepaldi s269551
 */
void FileSystemWatcher::recoverFromDB(client::Database *db, const std::function<void (Directory_entry&, FileSystemStatus)> &action) {
    std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> f;

    //update the content of the paths_ map
    f = [this, action](const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string& hash){
        auto element = Directory_entry(path_to_watch, path, size, type, lastWriteTime, Hash(hash));
        paths_[element.getAbsolutePath()] = std::move(element);
    };
    //use the function for each element of the db
    db->forAll(f);

    //add each element in paths_ to the initial modified events (to verify at the start that the server has already a copy of all the elements)
    for(auto i: paths_) {
        if(std::filesystem::exists(i.first))
            //check if the element exists in the filesystem, if yes put the element as modified in the elements queue;
            //otherwise it will be deleted so do not send anything (otherwise the element will be created on server and then deleted)
            action(i.second, FileSystemStatus::modified);
    }
}