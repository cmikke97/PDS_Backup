//
// Created by michele on 25/07/2020.
//

#include "Directory_entry.h"

/**
 * default constructor (it is needed by the pair class in map)
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(): size(0), type(Directory_entry_TYPE::notAType) {
}

/**
 * constructor with a filesystem::directory_entry as input
 *
 * @param entry std::filesystem::directory_entry to be represented
 *
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(const std::filesystem::directory_entry& entry):
        Directory_entry(
                entry.path().string(),
                entry.path().filename().string(),
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
 * @author Michele Crepaldi s269551
 */
Directory_entry::Directory_entry(std::string path, std::string name, uintmax_t size, Directory_entry_TYPE type,
                                 std::filesystem::file_time_type file_time): path(std::move(path)), name(std::move(name)), type(type){

    size = type==Directory_entry_TYPE::file?size:0;

    std::time_t tt = to_time_t(file_time);
    std::tm *gmt = std::gmtime(&tt);
    std::stringstream buffer;
    buffer << std::put_time(gmt, "%Y/%m/%d-%H:%M:%S");
    last_write_time = buffer.str();

    if(type==Directory_entry_TYPE::file) {
        std::stringstream temp;
        temp << path << size << last_write_time;
        hash = Hash{reinterpret_cast<const unsigned char *>(temp.str().c_str()), temp.str().length()};
    }
    else if(type==Directory_entry_TYPE::directory){
        std::stringstream temp;
        temp << path;
        hash = Hash{reinterpret_cast<const unsigned char *>(temp.str().c_str()), temp.str().length()};
    }
}

/**
 * get directory entry path
 *
 * @return path string
 *
 * @author Michele Crepaldi s269551
 */
std::string Directory_entry::getPath() {
    return path;
}

/**
 * get directory entry name
 *
 * @return name string
 *
 * @author Michele Crepaldi s269551
 */
std::string Directory_entry::getName() {
    return name;
}

/**
 * get directory entry size
 *
 * @return size
 *
 * @author Michele Crepaldi s269551
 */
uintmax_t Directory_entry::getSize() {
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
 * assign previous hash to this element (before edit)
 *
 * @param prev hash
 *
 * @author Michele Crepaldi s269551
 */
void Directory_entry::assignPrevHash(Hash prev) {
    prevHash = prev;
}

/**
 * get the previous hash for this element (before edit)
 *
 * @return previous hash
 *
 * @author Michele Crepaldi s269551
 */
Hash& Directory_entry::getPrevHash() {
    return prevHash;
}
