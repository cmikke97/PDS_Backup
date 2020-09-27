//
// Created by michele on 25/07/2020.
//

#include <iomanip>
#include <fstream>
#include <utility>
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
 * @param entry std::filesystem::path of the element to be represented
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::filesystem::path &path) :
        Directory_entry(std::filesystem::directory_entry(path)){
}

/**
 * constructor with a filesystem::directory_entry as input
 *
 * @param entry std::filesystem::directory_entry to be represented
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::filesystem::directory_entry& entry) :
        Directory_entry(
                entry.path(),
                entry.is_regular_file()?entry.file_size():0,
                entry.is_regular_file()?Directory_entry_TYPE::file:(entry.is_directory()?Directory_entry_TYPE::directory:Directory_entry_TYPE::notAType),
                entry.last_write_time()){
}

/**
 * constructor with all the parameters
 *
 * @param path of the directory entry
 * @param name of the directory entry
 * @param size of the file (only to be used if this is a file)
 * @param type file or directory
 * @param file_time last modification time
 *
 * @throw filesystem_error (if std::filesystem::relative fails)
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::filesystem::path& path, uintmax_t size, Directory_entry_TYPE type, std::filesystem::file_time_type file_time) :
                                 relativePath(std::filesystem::relative(path,baseDir).string()), absolutePath(path.string()), type(type){

    //if it is a file then set its size, if it is a directory then size is 0
    this->size = type==Directory_entry_TYPE::file?size:0;

    //convert file time to string
    std::time_t tt = to_time_t(file_time);
    std::tm *gmt = std::gmtime(&tt);
    std::stringstream buffer;
    buffer << std::put_time(gmt, "%Y/%m/%d-%H:%M:%S");
    last_write_time = buffer.str();

    std::stringstream temp;
    temp << relativePath;

    //if the entry is a file then compute its hash
    if(type==Directory_entry_TYPE::file)
        temp << this->size;

    temp << last_write_time;
    hash = Hash(temp.str());
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
 * set the base directory as the one given as argument
 *
 * @param dir
 *
 * @author Michele Crepaldi s269551
 */
void Directory_entry::setBaseDir(std::string dir) {
    baseDir = std::move(dir);
}
