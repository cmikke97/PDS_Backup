//
// Created by Michele Crepaldi s269551 on 25/07/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#ifndef DIRECTORY_ENTRY_H
#define DIRECTORY_ENTRY_H

#include <string>
#include <filesystem>
#include <iostream>

#include "Hash.h"


/**
 * Directory_entry_TYPE class: it describes (enumerically) all the relevant directory entry types
 *
 * @author Michele Crepaldi s269551
 */
enum class Directory_entry_TYPE {
    //default value (not a directory, nor a file)
    notFileNorDirectory,

    //directory
    directory,

    //regular file
    file
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Directory_entry class
 */

/**
 * Directory_entry class. Used to represent a directory entry and its relevant fields;
 *  (it also calculates the hash for files)
 *
 * @author Michele Crepaldi s269551
 */
class Directory_entry {
public:
    Directory_entry(const Directory_entry &) = default;             //default copy constructor
    Directory_entry& operator=(const Directory_entry &) = default;  //default copy assignment
    Directory_entry(Directory_entry &&) = default;                  //default move constructor
    Directory_entry& operator=(Directory_entry &&) = default;       //default move assignment
    ~Directory_entry() = default;                                   //default destructor


    //empty constructor
    Directory_entry();

    //constructor overload with base path and element's absolute path
    Directory_entry(const std::string &base, const std::string &absolutePath);

    //constructor overload with the base path and the element as directory_entry
    Directory_entry(const std::string &base, const std::filesystem::directory_entry &entry);

    //constructor overload with the base path, element's absolute path, size and type
    Directory_entry(const std::string &base, std::string absolutePath, uintmax_t size,
                    Directory_entry_TYPE type);

    //constructor overload with the base path, element's absolute path, size, type, last write time and hash
    Directory_entry(const std::string &base, const std::string &realtivePath, uintmax_t size, const std::string &type,
                    std::string lastWriteTime, Hash h);


    bool operator==(Directory_entry &other);    //operator== override

    //getters
    std::string& getRelativePath();
    std::string& getAbsolutePath();
    uintmax_t getSize() const;
    Directory_entry_TYPE getType();
    std::string& getLastWriteTime();
    Hash& getHash();

    //type checkers
    bool is_regular_file();
    bool is_directory();

    //time related methods
    std::string get_time_from_file();               //get the last write time of this element from filesystem
    void set_time_to_file(const std::string &time); //set the last write time of this element to filesystem

    //utility methods
    bool exists();       //method to know if this Directory_element actually exists on filesystem
    void updateValues(); //method used to update this Directory_element's info from filesystem (using its absolute path)

private:
    std::string _relativePath;      //directory entry relative path (relative to base path)
    std::string _absolutePath;      //directory entry absolute path
    uintmax_t _size{};              //directory entry size (0 for directories)
    Directory_entry_TYPE _type;     //directory entry type
    std::string _lastWriteTime;     //directory entry last write time
    Hash _hash;                     //directory entry hash (all zeros for directories)
};


#endif //DIRECTORY_ENTRY_H
