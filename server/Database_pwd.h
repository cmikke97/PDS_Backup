//
// Created by Michele Crepaldi s269551 on 21/10/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#ifndef SERVER_DATABASE_PWD_H
#define SERVER_DATABASE_PWD_H

#include <sqlite3.h>
#include <string>
#include <functional>
#include <mutex>
#include <memory>

#include "../myLibraries/Hash.h"

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
     * @tparam T class of the object to delete
     *
     * @author Michele Crepaldi s269551
     */
    template<class T>
    struct DeleterOf_pwd;

    /**
     * template specialization for the class sqlite3
     *
     * @author Michele Crepaldi s269551
     */
    template<>
    struct DeleterOf_pwd<sqlite3> {
        void operator()(sqlite3 *p) const { sqlite3_close(p); }
    };

    /*
     * definition of the UniquePtr construct, it is simply a unique_ptr object with the deleter for the
     * sqlite3Type template class redefined
     *
     * @author Michele Crepaldi s269551
     */
    template<class sqlite3Type>
    using UniquePtr_pwd = std::unique_ptr<sqlite3Type, DeleterOf_pwd<sqlite3Type>>;

    /**
     * pwd_databaseError class: it describes (enumerically) all the possible database errors
     *
     * @author Michele Crepaldi s269551
     */
    enum class DatabaseError_pwd {
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
     * +---------------------------------------------------------------------------------------------------------------+
     * Password Database class
     */

    /**
     * Password Database class. It represents an sqlite3 database (singleton)
     *
     * @author Michele Crepaldi s269551
     */
    class Database_pwd {
    public:
        Database_pwd(Database_pwd &) = delete; //copy constructor deleted
        Database_pwd& operator=(const Database_pwd &) = delete;  //assignment deleted
        Database_pwd(Database_pwd &&) = delete; //move constructor deleted
        Database_pwd& operator=(Database_pwd &&) = delete;  //move assignment deleted
        ~Database_pwd() = default;

        static void setPath(std::string path);

        //singleton instance getter
        static std::shared_ptr<Database_pwd> getInstance();

        //database methods

        std::pair<std::string, Hash> getHash(const std::string &username);
        void addUser(const std::string &username, const std::string &password);
        void updateUser(const std::string &username, const std::string &password);
        void removeUser(const std::string &username);
        void forAll(std::function<void(const std::string &)> &f);

    protected:
        //protected constructor
        Database_pwd();

        //mutex to synchronize threads during the first creation of the Singleton object
        static std::mutex mutex_;

        //singleton instance
        static std::shared_ptr<Database_pwd> database_;

        //path of the database file
        static std::string path_;

    private:
        //unique pointer to the actual database
        UniquePtr_pwd <sqlite3> _db;

        //access mutex to synchronize threads during database accesses
        std::mutex _access_mutex;

        void _open(); //database open function
        void _handleSQLError(int rc, int check, std::string &&message, DatabaseError_pwd err);   //error handler function
    };

    /*
     * +---------------------------------------------------------------------------------------------------------------+
     * DatabaseException class
     */

    /**
     * DatabaseException_pwd exception class that may be returned by the Database_pwd class
     * (derives from runtime_error)
     *
     * @author Michele Crepaldi s269551
     */
    class DatabaseException_pwd : public std::runtime_error {
    public:

        /**
         * database exception constructor
         *
         * @param msg the error message
         *
         * @author Michele Crepaldi s269551
         */
        explicit DatabaseException_pwd(const std::string &msg, DatabaseError_pwd code) :
                std::runtime_error(msg), _code(code) {
        }

        /**
         * database exception destructor
         *
         * @author Michele Crepaldi s269551
         */
        ~DatabaseException_pwd() noexcept override = default;

        /**
        * function to retrieve the error code from the exception
        *
        * @return database error code
        *
        * @author Michele Crepaldi s269551
        */
        DatabaseError_pwd getCode() const noexcept {
            return _code;
        }

    private:
        DatabaseError_pwd _code; //code describing the error
    };
}


#endif //SERVER_DATABASE_PWD_H
