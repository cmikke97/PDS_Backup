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
#include <atomic>
#include "../myLibraries/Directory_entry.h"
#include "Database.h"
#include <sqlite3.h>

/**
 * define all possible filesystem modifications to be watched
 *
 * @author Michele Crepaldi s269551
 */
enum class FileSystemStatus {created, deleted, modified, storeSent, modifySent};

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

    FileSystemWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay);
    void start(const std::function<bool (Directory_entry&, FileSystemStatus)> &action, std::atomic<bool> &stop);
    void recoverFromDB(client::Database *db, const std::function<void (Directory_entry&, FileSystemStatus)> &action);

private:
    std::unordered_map<std::string, Directory_entry> paths_;

    //bool contains(const std::string &key);
};


#endif //PDS_BACKUP_FILESYSTEMWATCHER_H
