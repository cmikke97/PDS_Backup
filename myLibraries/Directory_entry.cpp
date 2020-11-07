//
// Created by michele on 25/07/2020.
//

#include <iomanip>
#include <fstream>
#include <utility>
#include <regex>
#include "Directory_entry.h"

#define MAXBUFFSIZE 1024

/**
 * default constructor (it is needed by the pair class in map)
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(): size(0), type(Directory_entry_TYPE::notAType) {
}

/**
 * constructor with a filesystem::path as input
 *
 * @param basePath base path of the entry
 * @param entry std::filesystem::path of the element to be represented
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string &basePath, const std::string &absolutePath) :
        Directory_entry(
                basePath,
                std::filesystem::directory_entry(absolutePath)){
}

/**
 * constructor with a filesystem::directory_entry as input
 *
 * @param basePath base path of the entry
 * @param entry std::filesystem::directory_entry to be represented
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string &basePath, const std::filesystem::directory_entry& entry) :
        Directory_entry(
                basePath,
                entry.path(),
                entry.is_regular_file()?entry.file_size():0,
                entry.is_regular_file()?Directory_entry_TYPE::file:(entry.is_directory()?Directory_entry_TYPE::directory:Directory_entry_TYPE::notAType)){
}

/**
 * constructor with all the parameters
 *
 * @param basePath base path of the entry
 * @param path absolute path of the directory entry
 * @param size size of the file (only to be used if this is a file)
 * @param type type od the entry: file or directory
 * @param file_time last modification time
 *
 * @throw filesystem_error (if std::filesystem::relative fails)
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string& basePath, const std::string& absolutePath, uintmax_t size, Directory_entry_TYPE type) :
        absolutePath(absolutePath),
        type(type){

    //get relative path from absolutePath and baseDir
    std::smatch m;
    std::regex e ("(" + std::regex_replace(basePath, std::regex("\\/"), "\\/") + ")(.*)");

    if(std::regex_match (absolutePath,m,e))
        relativePath = m[2];
    else
        throw std::runtime_error("Error getting relative from absolute path"); //I should never arrive here

    //if it is a file then set its size, if it is a directory then size is 0
    this->size = type==Directory_entry_TYPE::file?size:0;

    //get last write time from the file and convert it to string
    last_write_time = get_time_from_file();

    if(type == Directory_entry_TYPE::file){
        //calculate hash
        HashMaker hm;

        /* TODO evaluate if to add these
        hm.update(relativePath);
        hm.update(sizeStr.str());
        hm.update(this->last_write_time);
        */

        std::ifstream infile;
        infile.open(absolutePath, std::ifstream::in | std::ifstream::binary);
        if(infile.is_open()){
            while(infile){
                char buf[MAXBUFFSIZE] = {0};    //I need this buffer to be all 0s for every line
                infile.getline(buf, MAXBUFFSIZE);
                hm.update(buf, MAXBUFFSIZE);
            }
        }

        hash = hm.get();
    }
}

/**
 * alternative constructor useful when handling data retrieved from a db
 *
 * @param basePath base path of the entry
 * @param relativePath relative path of the entry
 * @param size size of the entry
 * @param type type of the entry (file or directory)
 * @param lastWriteTime lastWriteTime of the entry in textual format
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::string& basePath, const std::string& relativePath, uintmax_t size, const std::string &type, std::string  lastWriteTime, Hash h):
        relativePath(relativePath),
        absolutePath(basePath + relativePath),
        last_write_time(std::move(lastWriteTime)),
        size(size),
        hash(h){

    if(type == "file")
        this->type = Directory_entry_TYPE::file;
    else if(type == "directory")
        this->type = Directory_entry_TYPE::directory;
    else
        this->type = Directory_entry_TYPE::notAType;
}

/**
 * operator == redefinition for the Directory_entry class
 *
 * @param other the other Directory_entry to compare this to
 * @return true if the two Directory entries are equal, false otherwise
 *
 * @author Michele Crepaldi s269551
 */
bool Directory_entry::operator==(Directory_entry &other){
    if(this->getType() != other.getType())
        return false;

    if(this->getAbsolutePath() != other.getAbsolutePath())
        return false;

    if(this->getRelativePath() != other.getRelativePath())
        return false;

    if(this->getSize() != other.getSize())
        return false;

    if(this->getLastWriteTime() != other.getLastWriteTime())
        return false;

    if(this->getHash() != other.getHash())
        return false;

    return true;
}

/**
 * get directory entry path
 *
 * @return path string
 *
 * @author Michele Crepaldi s269551
 */
std::string Directory_entry::getRelativePath() {
    return relativePath;
}

/**
 * get directory entry name
 *
 * @return name string
 *
 * @author Michele Crepaldi s269551
 */
std::string Directory_entry::getAbsolutePath() {
    return absolutePath;
}

/**
 * get directory entry size
 *
 * @return size
 *
 * @author Michele Crepaldi s269551
 */
uintmax_t Directory_entry::getSize() const {
    return size;
}

/**
 * get directory entry type
 *
 * @return type
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry_TYPE Directory_entry::getType() {
    return type;
}

/**
 * get directory entry last write time
 *
 * @return last write time
 *
 * @author Michele Crepaldi s269551
 */
std::string Directory_entry::getLastWriteTime() {
    return last_write_time;
}

/**
 *
 * @return true if this represents a file, false otherwise
 *
 * @author Michele Crepaldi s269551
 */
bool Directory_entry::is_regular_file() {
    return type == Directory_entry_TYPE::file;
}

/**
 *
 * @return true if this represents a directory, false otherwise
 *
 * @author Michele Crepaldi s269551
 */
bool Directory_entry::is_directory() {
    return type == Directory_entry_TYPE::directory;
}

/**
 * get the Hash associated to this directory entry
 *
 * @return Hash object associated to this element
 *
 * @author Michele Crepaldi s269551
 */
Hash& Directory_entry::getHash() {
    return hash;
}

/**
 * Utility function to get the last write (modify) time of a file as string;
 * <p> It is needed until c++20 because until then the lastWriteTime function of std::filesystem returns a file_time_type
 * variable which cannot be portably converted to a time_t nor to a string and then back
 *
 * @param path of the file to get the last write time of
 * @return string (readable) representation of the time got
 *
 * @throw runtime_error in case of errors with stat function
 *
 * @author Michele Crepaldi s269551
 */
std::string Directory_entry::get_time_from_file(){
    setenv("TZ", "UTC", 1);
    struct stat buf{};
    if(stat(absolutePath.data(), &buf) != 0)    //get file info
        throw std::runtime_error("Error in retrieving file info"); //throw exception in case of errors

    std::time_t tt = buf.st_mtime;  //get file last modified time
    std::tm *gmt = std::localtime(&tt); //convert std::time_t to std::tm
    std::stringstream buffer;
    buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M %Z"); //convert std::tm to a readable string
    return buffer.str();
}

/**
 * Utility function to set the last write (modify) time of a file given as string (readable);
 * <p> It is needed until c++20 because until then the lastWriteTime function of std::filesystem returns a file_time_type
 * variable which cannot be portably converted to a time_t nor to a string and then back
 *
 * @param absolutePath of the file to get the last write time of
 * @param time string (readable) representation of the time to use
 *
 * @throw runtime_error in case of errors with stat or utime functions
 *
 * @author Michele Crepaldi s269551
 */
void Directory_entry::set_time_to_file(const std::string &time){
    setenv("TZ", "UTC", 1);
    struct stat buf{};
    if(stat(absolutePath.data(), &buf) != 0)    //get file info
        throw std::runtime_error("Error in retrieving file info"); //throw exception in case of errors

    std::tm gmt{};
    std::stringstream buffer;
    buffer << time;
    buffer >> std::get_time(&gmt, "%A, %d %B %Y %H:%M %Z");    //convert from readable string to std::tm
    std::time_t tt = mktime(&gmt);  //convert std::tm to time_t

    struct utimbuf new_times{};
    new_times.actime = buf.st_atime; //keep atime unchanged
    new_times.modtime = tt; //set mtime to the time given as input
    if(utime(absolutePath.data(), &new_times) != 0) //set the new times for the file
        throw std::runtime_error("Error in setting file time"); //throw exception in case of errors


    //TODO choose between these 2
    //update the current value of last_write_time
    //last_write_time = time;
    //get last write time from the file and convert it to string
    last_write_time = get_time_from_file();
}

bool Directory_entry::exists(){
    return std::filesystem::exists(absolutePath);
}

void Directory_entry::updateValues(){
    std::filesystem::directory_entry entry{absolutePath};
    size = entry.is_regular_file()?entry.file_size():0;
    type = entry.is_regular_file()?Directory_entry_TYPE::file:(entry.is_directory()?Directory_entry_TYPE::directory:Directory_entry_TYPE::notAType);

    //get last write time from the file and convert it to string
    last_write_time = get_time_from_file();

    if(type == Directory_entry_TYPE::file){
        //calculate hash
        HashMaker hm;

        /* TODO evaluate if to add these
        hm.update(relativePath);
        hm.update(sizeStr.str());
        hm.update(this->last_write_time);
        */

        std::ifstream infile;
        infile.open(absolutePath, std::ifstream::in | std::ifstream::binary);
        if(infile.is_open()){
            while(infile){
                char buf[MAXBUFFSIZE] = {0};    //I need this buffer to be all 0s for every line
                infile.getline(buf, MAXBUFFSIZE);
                hm.update(buf, MAXBUFFSIZE);
            }
        }

        hash = hm.get();
    }
}