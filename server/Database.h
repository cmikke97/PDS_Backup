//
// Created by michele on 21/10/2020.
//

#ifndef SERVER_DATABASE_H
#define SERVER_DATABASE_H


#include <sqlite3.h>
#include <string>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <functional>
#include <utility>
#include <mutex>
#include "../myLibraries/Directory_entry.h"
#include "../myLibraries/RandomNumberGenerator.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * namespace
 */

/**
 * server namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace server {
    template<class T>
    struct DeleterOf;

    template<>
    struct DeleterOf<sqlite3> {
        void operator()(sqlite3 *p) const { sqlite3_close(p); }
    };

    template<class sqlite3Type>
    using UniquePtr = std::unique_ptr<sqlite3Type, DeleterOf<sqlite3Type>>;

    /**
     * databaseError class: it describes (enumerically) all the possible database errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class databaseError {
        open, create, read, insert, update, remove, prepare, finalize
    };

    /*
     * +-------------------------------------------------------------------------------------------------------------------+
     * Database class
     */

    /**
     * class which represents a sqlite3 database (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class Database {
        UniquePtr<sqlite3> db;
        std::mutex access_mutex;

        void open(const std::string &path);
        void handleSQLError(int rc, int check, std::string &&message, databaseError err);

    protected:
        explicit Database(std::string path);

        //to sincronize threads during the first creation of the Singleton object
        static std::mutex mutex_;
        //singleton instance
        static std::shared_ptr<Database> database_;
        std::string path_;

    public:
        Database(Database *other) = delete;
        void operator=(const Database &) = delete;

        static std::shared_ptr<Database> getInstance(const std::string &path);

        ~Database() = default;
        void forAll(std::string &username, std::string &mac, std::function<void(const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> &f);
        void insert(std::string &username, std::string &mac, const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash);
        void insert(std::string &username, std::string &mac, Directory_entry &d);
        void remove(std::string &username, std::string &mac, const std::string &path);
        void removeAll(std::string &username);
        void removeAll(std::string &username, std::string &mac);
        std::vector<std::string> getAllMacAddresses(const std::string &username);
        void update(std::string &username, std::string &mac, const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash);
        void update(std::string &username, std::string &mac, Directory_entry &d);
    };

    /*
     * +-------------------------------------------------------------------------------------------------------------------+
     * DatabaseException class
     */

    /**
     * exceptions for the database class
     *
     * @author Michele Crepaldi s269551
     */
    class DatabaseException : public std::runtime_error {
        databaseError code;
    public:

        /**
         * database exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        explicit DatabaseException(const std::string &msg, databaseError code) :
                std::runtime_error(msg), code(code) {
        }

        /**
         * database exception destructor.
         *
         * @author Michele Crepaldi s269551
         */
        ~DatabaseException() noexcept override = default;

        /**
        * function to retrieve the error code from the exception
        *
        * @return error code
        *
        * @author Michele Crepaldi s269551
        */
        databaseError getCode() const noexcept {
            return code;
        }
    };
}

#endif //SERVER_DATABASE_H
