//
// Created by Michele Crepaldi s269551 on 25/07/2020
// Finished on 22/11/2020
// Last checked on 22/11/2020
//

#ifndef CLIENT_FILESYSTEMWATCHER_H
#define CLIENT_FILESYSTEMWATCHER_H

#include <chrono>
#include <string>
#include <map>
#include <atomic>

#include "../myLibraries/Directory_entry.h"
#include "Database.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * FileSystemWatcher class
 */

/**
 * FileSystemStatus class: it describes (enumerically) all the possible filesystem modifications to be watched
 *
 * @author Michele Crepaldi s269551
 */
enum class FileSystemStatus {
    //element was createad
    created,

    //element was deleted
    deleted,

    //element was modified
    modified,

    //element STOR message was sent
    storeSent
};

/**
 * FileSystemWatcher class. Used to use to watch the file system for changes
 *
 * @author Michele Crepaldi s269551
 */
class FileSystemWatcher {
public:
    FileSystemWatcher(const FileSystemWatcher &) = delete;              //copy constructor deleted
    FileSystemWatcher& operator=(const FileSystemWatcher &) = delete;   //copy assignment deleted
    FileSystemWatcher(FileSystemWatcher &&) = delete;                   //move constructor deleted
    FileSystemWatcher& operator=(FileSystemWatcher &&) = delete;        //move assignment deleted
    ~FileSystemWatcher() = default; //default destructor

    //constructor with path to watch and interval
    FileSystemWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> interval);

    //start method (with action function and stop atomic boolean)
    void start(const std::function<bool (Directory_entry&, FileSystemStatus)> &action, std::atomic<bool> &stop);

    //recover from db method (with database and action function)
    void recoverFromDB(client::Database *db, const std::function<void (Directory_entry&, FileSystemStatus)> &action);

private:

    std::string _path_to_watch;  //path of the directory to watch

    //Time to wait between checks (for changes) on the folder to watch
    std::chrono::duration<int, std::milli> _interval;

    std::map<std::string, Directory_entry> _paths;  //map of saved directory entries
};


#endif //CLIENT_FILESYSTEMWATCHER_H
