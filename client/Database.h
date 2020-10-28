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
#include <utility>
#include <mutex>
#include "../myLibraries/Directory_entry.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * namespace
 */

/**
 * namespace containing the definition of the UniquePtr to use in the TLS classes
 *
 * @author Michele Crepaldi s269551
 */
namespace client {
    template<class T>
    struct DeleterOf;

    template<>
    struct DeleterOf<sqlite3> {
        void operator()(sqlite3 *p) const { sqlite3_close(p); }
    };

    template<class sqlite3Type>
    using UniquePtr = std::unique_ptr<sqlite3Type, DeleterOf<sqlite3Type>>;

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

        void open(const std::string &path);

    protected:
        explicit Database(std::string path);

        //to sincronize threads during the first creation of the Singleton object
        static std::mutex mutex_;
        //singleton instance
        static std::weak_ptr<Database> database_;
        std::string path_;

    public:
        Database(Database *other) = delete;

        void operator=(const Database &) = delete;

        static std::shared_ptr<Database> getInstance(const std::string &path);

        ~Database() = default;

        void forAll(std::function<void(const std::string &, const std::string &, uintmax_t, const std::string &,
                                       const std::string &)> &f);

        void insert(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime,
                    const std::string &hash);

        void insert(Directory_entry &d);

        void remove(const std::string &path);

        void update(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime,
                    const std::string &hash);

        void update(Directory_entry &d);
    };

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * DatabaseException class
 */

/**
 * databaseError class: it describes (enumerically) all the possible database errors
 *
 * @author Michele Crepaldi s269551
 */
    enum class databaseError {
        open, create, prepare, read, insert, remove, update
    };

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

#endif //CLIENT_DATABASE_H
