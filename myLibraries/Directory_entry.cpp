//
// Created by Michele Crepaldi s269551 on 25/07/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#include <iomanip>
#include <fstream>
#include <chrono>
#include <utime.h>
#include <regex>
#include "Directory_entry.h"

#define MAXBUFFSIZE 1024


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Directory_entry class
 */

/**
 * Directory_entry empty constructor
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(): _size(0), _type(Directory_entry_TYPE::notFileNorDirectory) {
}

/**
 * Directory_entry constructor overload with base path and element's absolute path
 *
 * @param basePath base path to be used for the relative path computation
 * @param absolutePath element's absolute path (in filesystem)
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string &basePath, const std::string &absolutePath) :
        Directory_entry(basePath, std::filesystem::directory_entry(absolutePath)){
}

/**
 * Directory_entry constructor overload with the base path and the element as directory_entry
 *
 * @param basePath base path to be used for the relative path computation
 * @param entry element as directory_entry
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string &basePath, const std::filesystem::directory_entry& entry) :
        Directory_entry(basePath, entry.path(), entry.is_regular_file()?entry.file_size():0,
                entry.is_regular_file()?Directory_entry_TYPE::file:
                (entry.is_directory()?Directory_entry_TYPE::directory:Directory_entry_TYPE::notFileNorDirectory)){
}

/**
 * Directory_entry constructor overload with the base path, element's absolute path, size and type
 *
 * @param basePath base path to be used for the relative path computation
 * @param absolutePath element's absolute path (in filesystem)
 * @param size element's size (0 for directories)
 * @param type element's type: file or directory (or notFileNorDirectory)
 *
 * @throw filesystem_error if the absolute path does not contain the base path, so the relative path could not
 *  be obtained
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string& basePath, std::string  absolutePath, uintmax_t size,
                                 Directory_entry_TYPE type) : _absolutePath(std::move(absolutePath)), _type(type){

    //get relative path from absolutePath and baseDir

    std::smatch m;  //regex match
    std::regex e ("(" + basePath + ")(.*)");    //regex to be used to extract the relative path from the absolute path

    //match the absolute path against the regex (does the absolute path contain the base path?)
    if(std::regex_match (_absolutePath,m,e))
        //if there was a match extract the relative path (group number 2)
        _relativePath = m[2];
    else
        //if the absolute path does not contain the basePath
        throw std::runtime_error("Could not obtain relative path from absolute path");

    //set the size, if the element is a file then use its size,
    //if it is a directory (or notFileNorDirectory) then the size is 0
    _size = _type == Directory_entry_TYPE::file ? size : 0;

    //get last write time from filesystem
    _lastWriteTime = get_time_from_file();

    //if the element si a file then calculate also its hash
    if(_type == Directory_entry_TYPE::file){
        HashMaker hm;

        std::ifstream file;
        file.open(_absolutePath, std::ifstream::in | std::ifstream::binary);   //open the file

        char buff[MAXBUFFSIZE]; //buffer

        if(file.is_open()){
            while(file.read(buff, MAXBUFFSIZE)) {  //get bytes from file
                hm.update(buff, file.gcount());    //update the HashMaker hash with the
            }
        }

        file.close();   //close the file

        _hash = hm.get();   //get the computed Hash
    }
}

/**
 * Directory_entry constructor overload with the base path, element's absolute path, size, type,
 *  last write time and hash
 *
 * @param basePath base path to be used for the relative path computation
 * @param absolutePath element's absolute path (in filesystem)
 * @param size element's size (0 for directories)
 * @param type element's type: file or directory (or notFileNorDirectory)
 * @param lastWriteTime element's lastWriteTime in textual format
 * @param hash element's hash
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string &basePath, const std::string &relativePath, uintmax_t size,
                                 const std::string &type, std::string lastWriteTime, Hash hash):
        _relativePath(relativePath),
        _absolutePath(basePath + relativePath),
        _lastWriteTime(std::move(lastWriteTime)),
        _size(size),
        _hash(hash){

    if(type == "file")
        this->_type = Directory_entry_TYPE::file;
    else if(type == "directory")
        this->_type = Directory_entry_TYPE::directory;
    else
        this->_type = Directory_entry_TYPE::notFileNorDirectory;
}

/**
 * Directory_entry class operator== override
 *
 * @param other the other Directory_entry to compare this Directory_entry with
 * @return true if the two Directory entries are equal, false otherwise
 *
 * @author Michele Crepaldi s269551
 */
bool Directory_entry::operator==(Directory_entry &other){
    if(this->getType() != other.getType())  //check the type
        return false;

    if(this->getAbsolutePath() != other.getAbsolutePath())  //check the absolute path
        return false;

    if(this->getRelativePath() != other.getRelativePath())  //check the relative path
        return false;

    if(this->getSize() != other.getSize())  //check the size
        return false;

    if(this->getLastWriteTime() != other.getLastWriteTime())    //check the last write time
        return false;

    if(this->getHash() != other.getHash())  //check the Hash
        return false;

    return true;
}

/**
 * directory entry relative path getter method
 *
 * @return element relative path
 *
 * @author Michele Crepaldi s269551
 */
std::string& Directory_entry::getRelativePath() {
    return _relativePath;
}

/**
 * directory entry absolute path getter method
 *
 * @return element absolute path
 *
 * @author Michele Crepaldi s269551
 */
std::string& Directory_entry::getAbsolutePath() {
    return _absolutePath;
}

/**
 * directory entry size getter method
 *
 * @return element size
 *
 * @author Michele Crepaldi s269551
 */
uintmax_t Directory_entry::getSize() const {
    return _size;
}

/**
 * directory entry type getter method
 *
 * @return element type
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry_TYPE Directory_entry::getType() {
    return _type;
}

/**
 * directory entry last write time getter method
 *
 * @return element last write time
 *
 * @author Michele Crepaldi s269551
 */
std::string& Directory_entry::getLastWriteTime() {
    return _lastWriteTime;
}

/**
 * directory entry type checker method, it checks if the element is a file
 *
 * @return whether this this element is a file or not
 *
 * @author Michele Crepaldi s269551
 */
bool Directory_entry::is_regular_file() {
    return _type == Directory_entry_TYPE::file;
}

/**
 * directory entry type checker method, it checks if the element is a directory
 *
 * @return whether this this element is a directory or not
 *
 * @author Michele Crepaldi s269551
 */
bool Directory_entry::is_directory() {
    return _type == Directory_entry_TYPE::directory;
}

/**
 * directory entry Hash getter method
 *
 * @return element Hash
 *
 * @author Michele Crepaldi s269551
 */
Hash& Directory_entry::getHash() {
    return _hash;
}

/**
 * Directory_entry utility method to get the last write (modify) time of a file as string;
 *  <p> It is needed until c++20 because for now the lastWriteTime function of std::filesystem returns a
 *  file_time_type variable which cannot be portably converted to a time_t nor to a string and then back
 *
 * @return string (readable) representation of the last write time got from filesystem
 *
 * @throw runtime_error in case of errors with stat function
 *
 * @author Michele Crepaldi s269551
 */
std::string Directory_entry::get_time_from_file(){
    setenv("TZ", "UTC", 1); //set "TZ" (Time zone) environment variable to "UTC"

    struct stat buf{};
    if(stat(_absolutePath.data(), &buf) != 0)    //get file info
        throw std::runtime_error("Error in retrieving directory entry info");

    std::time_t tt = buf.st_mtime;      //get file last modified time
    std::tm *gmt = std::localtime(&tt); //convert std::time_t to std::tm

    std::stringstream buffer;
    buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M %Z"); //convert std::tm to a readable string
    return std::move(buffer.str());
}

/**
 * Directory_entry utility method to set the last write (modify) time of a file given as string (readable);
 *  <p> It is needed until c++20 because for now the lastWriteTime function of std::filesystem returns a
 *  file_time_type variable which cannot be portably converted to a time_t nor to a string and then back
 *
 * @param time string (readable) representation of the time to set
 *
 * @throw runtime_error in case of errors with stat or utime functions
 *
 * @author Michele Crepaldi s269551
 */
void Directory_entry::set_time_to_file(const std::string &time){
    setenv("TZ", "UTC", 1); //set "TZ" (Time zone) environment variable to "UTC"

    struct stat buf{};
    if(stat(_absolutePath.data(), &buf) != 0)    //get file info
        throw std::runtime_error("Error in retrieving directory entry info");

    std::tm gmt{};
    std::stringstream buffer;
    buffer << time;
    buffer >> std::get_time(&gmt, "%A, %d %B %Y %H:%M %Z");    //convert from readable string to std::tm
    std::time_t tt = mktime(&gmt);  //convert std::tm to time_t

    struct utimbuf new_times{};
    new_times.actime = buf.st_atime;    //keep atime unchanged
    new_times.modtime = tt;             //set mtime to the time given as input
    if(utime(_absolutePath.data(), &new_times) != 0) //set the new times for the file
        throw std::runtime_error("Error in setting file time");

    //get last write time from the file and convert it to string (in order to update _lastWriteTime)
    _lastWriteTime = get_time_from_file();
}

/**
 * Directory_entry utility method, it checks if the element exists in the filesystem
 *
 * @return whether the element exists or not
 *
 * @author Michele Crepaldi s269551
 */
bool Directory_entry::exists(){
    return std::filesystem::exists(_absolutePath);
}

/**
 * Directory_entry utility method, it updates all the fields of this element using the values found in the
 *  filesystem (the element is identified by its absolute path)
 *
 * @throw runtime_error if the element does not exist anymore
 *
 * @author Michele Crepaldi s269551
 */
void Directory_entry::updateValues(){
    if(!std::filesystem::exists(_absolutePath))
        throw std::runtime_error("Error updating directory entry, it does not exist");

    std::filesystem::directory_entry entry{_absolutePath};  //actual entry in filesystem
    _size = entry.is_regular_file() ? entry.file_size() : 0;
    _type = entry.is_regular_file() ? Directory_entry_TYPE::file : (entry.is_directory() ? Directory_entry_TYPE::directory : Directory_entry_TYPE::notFileNorDirectory);

    //get last write time from filesystem
    _lastWriteTime = get_time_from_file();

    //if the element si a file then calculate also its hash
    if(_type == Directory_entry_TYPE::file){
        HashMaker hm;

        std::ifstream file;
        file.open(_absolutePath, std::ifstream::in | std::ifstream::binary);   //open the file

        char buff[MAXBUFFSIZE]; //buffer

        if(file.is_open()){
            while(file.read(buff, MAXBUFFSIZE)) {  //get bytes from file
                hm.update(buff, file.gcount());    //update the HashMaker hash with the
            }
        }

        file.close();   //close the file

        _hash = hm.get();   //get the computed Hash
    }
}