//
// Created by michele on 25/07/2020.
//

#ifndef CLIENT_DIRECTORY_ENTRY_H
#define CLIENT_DIRECTORY_ENTRY_H

#include <string>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <utility>
//#include <openssl/evp.h>
//#include <openssl/sha.h>

enum class Directory_entry_TYPE {directory, file,notAType};

template <typename TP>
std::time_t to_time_t(TP tp)
{
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
    return system_clock::to_time_t(sctp);
}

/**
 * class to represent a directory entry and its important fields
 * it is also used to calculate a hash for the directory entry
 *
 * @author Michele Crepaldi s269551
 */
class Directory_entry {

private:
    std::string path;
    std::string name;
    uintmax_t size{};
    Directory_entry_TYPE type;
    std::string last_write_time;
    //unsigned char hash[EVP_MAX_MD_SIZE];

    /**
     * TODO have the hash as another field of this class
     */

public:
    Directory_entry();
    explicit Directory_entry(const std::filesystem::directory_entry&);
    Directory_entry(std::string path, std::string name, uintmax_t size, Directory_entry_TYPE type, std::filesystem::file_time_type file_time);

    std::string getPath();
    std::string getName();
    uintmax_t getSize();
    Directory_entry_TYPE getType();
    std::string getLastWriteTime();
    bool is_regular_file();
    bool is_directory();

    /**
     * TODO implement hash function getter for this class (if the hash was never computed before then compute it, otherwise just return it)
     */

};


#endif //CLIENT_DIRECTORY_ENTRY_H
