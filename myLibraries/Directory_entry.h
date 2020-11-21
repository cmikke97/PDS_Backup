//
// Created by michele on 25/07/2020.
//

#ifndef DIRECTORY_ENTRY_H
#define DIRECTORY_ENTRY_H

//TODO check

#include <string>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <utility>
#include <utime.h>
#include "Hash.h"

enum class Directory_entry_TYPE {directory, file, notAType};

/**
 * class to represent a directory entry and its important fields
 * it is also used to calculate a hash for the directory entry
 *
 * @author Michele Crepaldi s269551
 */
class Directory_entry {

private:
    std::string relativePath;
    std::string absolutePath;
    uintmax_t size{};
    Directory_entry_TYPE type;
    std::string last_write_time;
    Hash hash;

public:
    Directory_entry();
    explicit Directory_entry(const std::string&, const std::string&);
    explicit Directory_entry(const std::string&, const std::filesystem::directory_entry&);

    Directory_entry(const std::string &base, const std::string& absolutePath, uintmax_t size, Directory_entry_TYPE type);
    Directory_entry(const std::string &base, const std::string& realtivePath, uintmax_t size, const std::string &type, std::string  lastWriteTime, Hash h);

    bool operator==(Directory_entry &other);

    std::string& getRelativePath();
    std::string& getAbsolutePath();
    uintmax_t getSize() const;
    Directory_entry_TYPE getType();
    std::string& getLastWriteTime();
    bool is_regular_file();
    bool is_directory();
    Hash& getHash();
    std::string get_time_from_file();
    void set_time_to_file(const std::string &time);
    bool exists();
    void updateValues();
};


#endif //DIRECTORY_ENTRY_H
