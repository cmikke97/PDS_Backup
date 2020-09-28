//
// Created by michele on 28/09/2020.
//

#ifndef CLIENT_DATABASE_H
#define CLIENT_DATABASE_H


#include <sqlite3.h>
#include <string>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <functional>

/**
 * class which represents a sqlite3 database
 *
 * @author Michele Crepaldi s269551
 */
class Database {
    sqlite3 *db{};

public:
    Database() = default;
    ~Database();
    void open(const std::string& path);
    void forAll(std::function<void (const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime)> &f);
    void insert(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime);
    void remove(const std::string &path);
    void update(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime);

};

/**
 * exceptions for the database class
 *
 * @author Michele Crepaldi s269551
 */
class DatabaseException : public std::runtime_error {
public:

    /**
     * database exception constructor
     *
     * @param msg the error message
     *
     * @author Michele Crepaldi s269551
     */
    DatabaseException(const std::string& msg):
            std::runtime_error(msg){
    }

    /**
     * database exception destructor.
     *
     * @author Michele Crepaldi s269551
     */
    ~DatabaseException() noexcept override = default;
};


#endif //CLIENT_DATABASE_H
