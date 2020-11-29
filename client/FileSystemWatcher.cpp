//
// Created by Michele Crepaldi s269551 on 25/07/2020
// Finished on 22/11/2020
// Last checked on 22/11/2020
//

#include "FileSystemWatcher.h"

#include <filesystem>
#include <thread>
#include <functional>


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * FileSystemWatcher class methods
 */

/**
 * FileSystemWatcher class constructor.
 *  Keep a record of files from the base directory and their last modification time
 *
 * @param path_to_watch folder this FileSystemWatcher has to watch
 * @param interval amount of time to wait between checks (for changes) on the path_to_watch
 *
 * @author Michele Crepaldi s269551
 */
FileSystemWatcher::FileSystemWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> interval) : _path_to_watch{std::move(path_to_watch)}, _interval{interval} {
}

/**
 * FileSystemWatcher start method.
 *  Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
 *
 * @param action action to be performed
 * @param stop atomic boolean to stop this FileSystemWatcher
 *
 * @author Michele Crepaldi s269551
 */
void FileSystemWatcher::start(const std::function<bool (Directory_entry&, FileSystemStatus)> &action, std::atomic<bool> &stop) {

    //loop until told to stop
    while(!stop.load()) {
        //check if a file/directory was deleted

        for(auto it = _paths.begin(); it != _paths.end(); ){
            if(!std::filesystem::exists(it->first)) {   //if the element does not exist any more perform the action

                //the element was deleted

                //if the action was successful then erase the element from _paths;
                //otherwise this element will be removed later (when its deletion will be re-detected)
                if(action(it->second, FileSystemStatus::deleted)) {
                    //std::map::erase returns an iterator to the element that follows the last element removed
                    it = _paths.erase(it);
                }
                else
                    it++;
            }
            else
                it++;
        }

        //Check if a file/directory was created or modified

        for(const auto& file : std::filesystem::recursive_directory_iterator(_path_to_watch)) {

            //if it is not a file nor a directory don't do anything and go on
            if(!file.is_directory() && !file.is_regular_file())
                continue;

            auto current = Directory_entry(_path_to_watch, file);   //current Directory_entry element

            //old (string,Directory_entry) pair corresponding to the current element path
            auto old = _paths.find(file.path().string());

            if(old == _paths.end()) { //if no element was found it means it has just been created
                //the element was created

                //if the action was successful then add the element to paths_;
                //otherwise this element will be added later (when its creation will be re-detected)
                if(action(current, FileSystemStatus::created))
                    _paths[current.getAbsolutePath()] = std::move(current);

            }else { //if an element was found then check if it was modified

                auto el = old->second;  //old Directory_entry element

                //compare all old Directory_entry member variables with the current ones
                if (el.getLastWriteTime() != current.getLastWriteTime() || el.getType() != current.getType() ||
                        el.getSize() != current.getSize() || el.getHash() != current.getHash()) {

                    //the element was modified

                    //if the action was successful then update the element in paths_;
                    //otherwise this element will be updated later (when its modification will be re-detected)
                    if (action(current, FileSystemStatus::modified))
                        old->second = std::move(current);
                }

            }
        }

        //Wait for _interval milliseconds
        std::this_thread::sleep_for(_interval);
    }
}

/**
 * FileSystemWatcher recover from db method.
 *  Used by to retrieve previously save data (about the entries) from the db
 *
 * @param db db to retrieve data from
 * @param action action to perform for each row of the db (corresponding to actually existing filesystem elements)
 *
 * @author Michele Crepaldi s269551
 */
void FileSystemWatcher::recoverFromDB(client::Database *db,
                                      const std::function<void (Directory_entry&, FileSystemStatus)> &action) {

    //function to apply on each row of the database to retrieve the information
    std::function<void (const std::string &, const std::string &, uintmax_t,
            const std::string &, const std::string &)> f;

    //update the content of the paths_ map
    f = [this](const std::string &path, const std::string &type, uintmax_t size,
            const std::string &lastWriteTime, const std::string& hash){

        //current element
        auto element = Directory_entry(_path_to_watch, path, size, type, lastWriteTime, Hash(hash));

        //insert it into the _paths map
        _paths[element.getAbsolutePath()] = std::move(element);
    };

    //use the function for each element of the db
    db->forAll(f);

    //perform the action on each (existing) element in paths_
    for(auto i: _paths)
        //check if the element exists in the filesystem
        if(std::filesystem::exists(i.first))    //if yes then perform the action
            action(i.second, FileSystemStatus::modified);
}