//
// Created by Michele Crepaldi s269551 on 21/10/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#ifndef SERVER_DATABASE_H
#define SERVER_DATABASE_H

#include <sqlite3.h>
#include <string>
#include <functional>
#include <mutex>

#include "../myLibraries/Directory_entry.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * namespace
 */

/**
 * PDS_Backup server namespace
 *
 * @author Michele Crepaldi s269551
 */
namespace server {
    /**
     * custom deleter template for class T
     *
     * @tparam T class of the object to delete
     *
     * @author Michele Crepaldi s269551
     */
    template<class T>
    struct DeleterOf;

    /**
     * template specialization for the sqlite3 class
     *
     * @author Michele Crepaldi s269551
     */
    template<>
    struct DeleterOf<sqlite3> {
        void operator()(sqlite3 *p) const { sqlite3_close(p); }
    };

    /*
     * definition of the UniquePtr construct, it is simply a unique_ptr object with the deleter for the
     * sqlite3Type template class redefined
     *
     * @author Michele Crepaldi s269551
     */
    template<class sqlite3Type>
    using UniquePtr = std::unique_ptr<sqlite3Type, DeleterOf<sqlite3Type>>;

    /**
     * databaseError class: it describes (enumerically) all the possible database errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class DatabaseError {
        //no path set
        path,

        //cannot open the database file
        open,

        //cannot create the database
        create,

        //cannot read the database
        read,

        //cannot insert row into database
        insert,

        //cannot update row in database
        update,

        //cannot remove row from database
        remove,

        //cannot prepare sql statement
        prepare,

        //cannot finalize sql statement
        finalize
    };

    /*
     * +-------------------------------------------------------------------------------------------------------------------+
     * Database class
     */

    /**
     * Database class. It represents an sqlite3 database (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class Database {
    public:
        Database(Database &) = delete; //copy constructor deleted
        Database& operator=(const Database &) = delete;  //assignment deleted
        Database(Database &&) = delete; //move constructor deleted
        Database& operator=(Database &&) = delete;  //move assignment deleted
        ~Database() = default;

        static void setPath(std::string path);

        //singleton instance getter
        static std::shared_ptr<Database> getInstance();

        //database methods

        void forAll(const std::string &username, const std::string &mac,
                    std::function<void(const std::string&, const std::string&, uintmax_t,
                            const std::string&, const std::string&)> &f);
        void insert(const std::string &username, const std::string &mac, const std::string &path,
                    const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash);
        void insert(const std::string &username, const std::string &mac, Directory_entry& d);
        void remove(const std::string &username, const std::string &mac, const std::string &path);
        void removeAll(const std::string &username);
        void removeAll(const std::string &username, const std::string &mac);
        std::vector<std::string> getAllMacAddresses(const std::string &username);
        void update(const std::string &username, const std::string &mac, const std::string &path,
                    const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash);
        void update(const std::string &username, const std::string &mac, Directory_entry &d);

    protected:
        //protected constructor
        Database();

        //mutex to synchronize threads during the first creation of the Singleton object
        static std::mutex mutex_;

        //singleton instance
        static std::shared_ptr<Database> database_;

        //path of the database file
        static std::string path_;

    private:
        //unique pointer to the actual database
        UniquePtr<sqlite3> _db;

        //access mutex to synchronize threads during database accesses
        std::mutex _access_mutex;

        void _open(); //database open function
        void _handleSQLError(int rc, int check, std::string &&message, DatabaseError err);   //error handler function
    };

    /*
     * +-------------------------------------------------------------------------------------------------------------------+
     * DatabaseException class
     */

    /**
     * DatabaseException exception class that may be returned by the Database class
     *  (derives from runtime_error)
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
        explicit DatabaseException(const std::string &msg, DatabaseError code) :
                std::runtime_error(msg), _code(code) {
        }

        /**
         * database exception destructor
         *
         * @author Michele Crepaldi s269551
         */
        ~DatabaseException() noexcept override = default;

        /**
        * function to retrieve the error code from the exception
        *
        * @return database error code
        *
        * @author Michele Crepaldi s269551
        */
        DatabaseError getCode() const noexcept {
            return _code;
        }

    private:
        DatabaseError _code; //code describing the error
    };
}

#endif //SERVER_DATABASE_H
