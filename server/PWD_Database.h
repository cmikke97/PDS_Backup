//
// Created by michele on 21/10/2020.
//

#ifndef SERVER_PWD_DATABASE_H
#define SERVER_PWD_DATABASE_H

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <functional>
#include <utility>
#include <mutex>
#include "../myLibraries/Hash.h"
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
    struct DeleterOf2;

    template<>
    struct DeleterOf2<sqlite3> {
        void operator()(sqlite3 *p) const { sqlite3_close(p); }
    };

    template<class sqlite3Type>
    using UniquePtr2 = std::unique_ptr<sqlite3Type, DeleterOf2<sqlite3Type>>;

    /*
     * +-------------------------------------------------------------------------------------------------------------------+
     * Password Database class
     */

    //TODO it needs to be thread safe in queries

    /**
     * class which represents a sqlite3 database (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class PWD_Database {
        UniquePtr2 <sqlite3> db;
        std::mutex access_mutex;

        void open(const std::string &path);

    protected:
        explicit PWD_Database(std::string path);

        //to sincronize threads during the first creation of the Singleton object
        static std::mutex mutex_;
        //singleton instance
        static std::shared_ptr<PWD_Database> database_;
        std::string path_;

    public:
        PWD_Database(PWD_Database *other) = delete;

        void operator=(const PWD_Database &) = delete;

        static std::shared_ptr<PWD_Database> getInstance(const std::string &path);

        ~PWD_Database() = default;

        std::pair<std::string, Hash> getHash(std::string &username);
        void addUser(const std::string &username, const std::string &password);
        void updateUser(const std::string &username, const std::string &password);
        void removeUser(const std::string &username);
        void forAll(std::function<void(const std::string &)> &f);
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
    enum class PWT_databaseError {
        open, create, read, insert, update, remove, prepare, hash
    };

    /**
     * exceptions for the database class
     *
     * @author Michele Crepaldi s269551
     */
    class PWT_DatabaseException : public std::runtime_error {
        PWT_databaseError code;
    public:

        /**
         * database exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        explicit PWT_DatabaseException(const std::string &msg, PWT_databaseError code) :
                std::runtime_error(msg), code(code) {
        }

        /**
         * database exception destructor.
         *
         * @author Michele Crepaldi s269551
         */
        ~PWT_DatabaseException() noexcept override = default;

        /**
        * function to retrieve the error code from the exception
        *
        * @return error code
        *
        * @author Michele Crepaldi s269551
        */
        PWT_databaseError getCode() const noexcept {
            return code;
        }
    };
}


#endif //SERVER_PWD_DATABASE_H
