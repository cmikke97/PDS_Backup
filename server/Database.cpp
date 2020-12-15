//
// Created by Michele Crepaldi s269551 on 21/10/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#include "Database.h"

#include <filesystem>
#include <fstream>

#include "../myLibraries/RandomNumberGenerator.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Database class methods
 */

//static variable definition
std::shared_ptr<server::Database> server::Database::database_;
std::mutex server::Database::mutex_;
std::string server::Database::path_;

/**
 * Database class path_ variable setter
 *
 * @param path path of the database on disk
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::setPath(std::string path){
    path_ = std::move(path);    //set the path_
}

/**
 * Database class singleton instance getter method
 *
 * @return Database instance
 *
 * @author Michele Crepaldi s269551
 */
std::shared_ptr<server::Database> server::Database::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(database_ == nullptr) //first time, or when it was released from everybody
        database_ = std::shared_ptr<Database>(new Database());  //create the database object
    return database_;
}

/**
 * (protected) constructor of the database object
 *
 * @throws DatabaseException:
 *  <b>path</b> if no path was set before this call
 *
 * @author Michele Crepaldi s269551
 */
server::Database::Database() {
    if(path_.empty())   //a path must be previously set
        throw DatabaseException("No path set", DatabaseError::path);

    _open();   //open the database from file
}

/**
 * method used to handle the SQL errors
 *
 * @param rc code returned from the SQLite function
 * @param check SQLite code to check the returned code against
 * @param message eventual error message to print (moved)
 * @param err databaseError to insert into the possible exception
 *
 * @throws DatabaseException:
 *  <b>[err]</b> in case rc != check
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::_handleSQLError(int rc, int check, std::string &&message, DatabaseError err){
    if(rc != check) {   //if there was an error
        std::stringstream tmp;

        //get the error message also from the sqlite3 object
        tmp << message << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(_db.get());

        throw server::DatabaseException(tmp.str(), err);    //throw exception
    }
}

/**
 * method used to open the connection to a sqlite3 database; if the database already exists then it opens it,
 *  otherwise it also creates the needed table
 *
 * @throws DatabaseException:
 *  <b>open</b> if the database could not be opened
 * @throws DatabaseException:
 *  <b>create</b> if the (new) database could not be created
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::_open() {
    int rc; //sqlite3 methods' return code

    //check if the db already exists before opening it (create a new db if none is found)

    bool dbExists = std::filesystem::exists(path_);

    if(!dbExists){ //if the file does not exist create it
        std::ofstream f;
        std::filesystem::path p{path_};
        auto parent = p.parent_path();  //parent path of path_

        //create all the directories (if they do not already exist) up to the parent path
        std::filesystem::create_directories(parent);

        f.open(path_, std::ios::out | std::ios::trunc);  //create the file
        f.close();  //close the file
    }

    //open the database

    sqlite3 *dbTmp; //pointer to the sqlite3 db
    rc = sqlite3_open(path_.c_str(), &dbTmp);   //open the database
    _db.reset(dbTmp);   //assign the newly opened db to the _db smart pointer

    _handleSQLError(rc, SQLITE_OK, "Cannot open database: ", DatabaseError::open);

    //if the db is new then create the table inside it
    if(!dbExists){
        //"CREATE" SQL statement
        std::string sql = "CREATE TABLE savedFiles ("
                          "id INTEGER,"
                          "username TEXT,"
                          "mac TEXT,"
                          "path TEXT,"
                          "size INTEGER,"
                          "type TEXT,"
                          "lastWriteTime TEXT,"
                          "hash TEXT,"
                          "PRIMARY KEY(id AUTOINCREMENT));";

        //Execute SQL statement
        rc = sqlite3_exec(_db.get(), sql.c_str(), nullptr, nullptr, nullptr);

        _handleSQLError(rc, SQLITE_OK, "Cannot create table: ", DatabaseError::create);
    }
}

/**
 * method used to apply a provided function to each row of the database for a specified user-mac pair
 *
 * @param username username
 * @param mac mac address of the client host
 * @param f function to be used for each row extracted from the database
 *
 * @throws DatabaseException:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException:
 *  <b>read</b> if the database could not be read
 * @throws DatabaseException:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::forAll(const std::string &username, const std::string &mac,
                const std::function<void (const std::string &, const std::string &,
                uintmax_t, const std::string &, const std::string &)> &f) {

    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code
    sqlite3_stmt* stmt; //statement handle

    //"SELECT" SQL statement
    std::string sql = "SELECT path, type, size, lastWriteTime, hash FROM savedFiles WHERE username=? AND mac=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError::prepare);

    //bind parameters
    sqlite3_bind_text(stmt, 1, username.c_str(), username.length(), SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt, 2, mac.c_str(), mac.length(), SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);


    bool done = false;
    //loop over table content
    while (!done) {
        switch (rc = sqlite3_step (stmt)) { //execute a step of the sql statement on the database
            case SQLITE_ROW:    //in case a database row was extracted
            {
                //get column values from the row (and convert them)

                //element path
                std::string path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
                //element type
                std::string type = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
                //element size
                uintmax_t size = sqlite3_column_int(stmt, 2);
                //element last write time
                std::string lastWriteTime = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
                //hex representation of the element hash
                std::string hashHex = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));

                //convert hash from hex representation (as it is stored in the database)
                //to bitstring representation (as it is used in the program)

                //bitstring representation of the element hash
                std::string hash = RandomNumberGenerator::hex_to_string(hashHex);

                //use provided function
                f(path, type, size, lastWriteTime, hash);
                break;
            }

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;

                //get the error message also from the sqlite3 object
                tmp << "Cannot read table: " << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(_db.get());

                throw DatabaseException(tmp.str(), DatabaseError::read);
        }
    }

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method used to insert a new element in the database for a specified user-mac pair
 *
 * @param username username
 * @param mac mac address of the client host
 * @param path path of the element to be inserted
 * @param type type of the element to be inserted
 * @param size size of the element to be inserted
 * @param lastWriteTime last write time of the element to be inserted
 *
 * @throws DatabaseException:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException:
 *  <b>insert</b> if the row could not be inserted into the database
 * @throws DatabaseException:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::insert(const std::string &username, const std::string &mac,
                              const std::string &path, const std::string &type,
                              uintmax_t size, const std::string &lastWriteTime, const std::string &hash) {

    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code

    //convert hash from bitstring representation (as it is used in the program)
    //to hex representation (as it is stored in the database)

    std::string hashHex = RandomNumberGenerator::string_to_hex(hash);   //hex representation of the element hash

    sqlite3_stmt* stmt; //statement handle

    //"INSERT" SQL statement
    std::string sql = "INSERT INTO savedFiles (username, mac, path, type, size, lastWriteTime, hash) VALUES (?,?,?,?,?,?,?);";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError::prepare);

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,2,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,3,path.c_str(),path.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,4,type.c_str(),type.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_int64(stmt,5,size);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,6,lastWriteTime.c_str(),lastWriteTime.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,7,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);

    //execute SQL statement
    rc = sqlite3_step(stmt);    //execute the one (and only) step of this statement on the database
    _handleSQLError(rc, SQLITE_DONE, "Cannot insert into savedFiles table: ", DatabaseError::insert);

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method used to insert a new element in the database for a specified user-mac pair
 *
 * @param username username
 * @param mac mac address of the client host
 * @param d Directory_entry to be inserted
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::insert(const std::string &username, const std::string &mac, Directory_entry &d) {
    std::string type;
    if(d.getType() == Directory_entry_TYPE::file)   //if the element is a file
        type = "file";
    else
        type = "directory";

    //insert the element into the database
    insert(username, mac, d.getRelativePath(), type, d.getSize(), d.getLastWriteTime(), d.getHash().str());
}

/**
 * method used to remove an element from the database for a specified user-mac pair
 *
 * @param username username
 * @param mac mac address of the client host
 * @param path path of the element to be removed
 *
 * @throws DatabaseException:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException:
 *  <b>remove</b> if the row could not be removed from the database
 * @throws DatabaseException:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::remove(const std::string &username, const std::string &mac, const std::string &path) {
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc;  //sqlite3 methods' return code

    sqlite3_stmt* stmt;  //statement handle

    //"DELETE" SQL statement
    std::string sql =   "DELETE FROM savedFiles WHERE path=? AND username=? AND mac=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError::prepare);

    //bind parameters
    sqlite3_bind_text(stmt,1,path.c_str(),path.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,2,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,3,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);

    //execute SQL statement
    rc = sqlite3_step(stmt);
    _handleSQLError(rc, SQLITE_DONE, "Cannot delete row from savedFiles table: ", DatabaseError::remove);

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method used to remove all elements from the database for a specified user
 *
 * @param username
 *
 * @throws DatabaseException:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException:
 *  <b>remove</b> if the rows could not be removed from the database
 * @throws DatabaseException:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::removeAll(const std::string &username){
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code

    sqlite3_stmt* stmt; //statement handle

    //"DELETE" SQL statement
    std::string sql =   "DELETE FROM savedFiles WHERE username=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError::prepare);

    //bind parameter
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);

    //execute SQL statement
    rc = sqlite3_step(stmt);
    _handleSQLError(rc, SQLITE_DONE, "Cannot remove (user) entries from savedFiles table: ", DatabaseError::remove);

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method used to remove all elements from the database for a specified user-mac pair
 *
 * @param username
 * @param mac mac address of the client host
 *
 * @throws DatabaseException:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException:
 *  <b>remove</b> if the rows could not be removed from the database
 * @throws DatabaseException:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::removeAll(const std::string &username, const std::string &mac){
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code

    sqlite3_stmt* stmt; //statement handle

    //"DELETE" SQL statement
    std::string sql =   "DELETE FROM savedFiles WHERE username=? AND mac=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError::prepare);

    //bind parameter
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,2,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);

    //execute SQL statement
    rc = sqlite3_step(stmt);
    _handleSQLError(rc, SQLITE_DONE, "Cannot remove (user-mac) entries from savedFiles table: ", DatabaseError::remove);

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method used to get all the registered mac addresses from the database for a specified user-mac pair
 *
 * @param username
 *
 * @return vector of strings containing all mac addresses associated to the provided user
 *
 * @throws DatabaseException:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException:
 *  <b>read</b> if the database could not be read
 * @throws DatabaseException:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
std::vector<std::string> server::Database::getAllMacAddresses(const std::string &username){
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code

    sqlite3_stmt* stmt; //statement handle

    //"SELECT" SQL statement
    std::string sql = "SELECT DISTINCT mac FROM savedFiles WHERE username=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError::prepare);

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);


    std::vector<std::string> macAddrs;  //list of all user's macs

    bool done = false;
    //loop over table content
    while (!done) {
        switch (rc = sqlite3_step (stmt)) { //execute a step of the sql statement on the database
                case SQLITE_ROW:    //in case a database row was extracted
            {
                //convert column and add mac address to vector
                macAddrs.emplace_back(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
                break;
            }

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;

                //get the error message also from the sqlite3 object
                tmp << "Cannot read table: " << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(_db.get());

                throw DatabaseException(tmp.str(), DatabaseError::read);
        }
    }

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);

    //return (by moving it) the vector of mac addresses for the provided user
    return std::move(macAddrs);
}

/**
 * method used to update an element of the database for a specified user-mac pair
 *
 * @param username username
 * @param mac mac address of the client host
 * @param path path of the element to be updated
 * @param type type of the element to be updated
 * @param size size of the element to be updated
 * @param lastWriteTime last write time of the element to be updated
 *
 * @throws DatabaseException:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException:
 *  <b>update</b> if the element could not be updated in the database
 * @throws DatabaseException:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::update(const std::string &username, const std::string &mac,
                              const std::string &path, const std::string &type, uintmax_t size,
                              const std::string &lastWriteTime, const std::string &hash) {

    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code

    //convert hash from bitstring representation (as it is used in the program)
    //to hex representation (as it is stored in the database)

    std::string hashHex = RandomNumberGenerator::string_to_hex(hash);   //hex representation of the element hash

    sqlite3_stmt* stmt; //statement handle

    //"UPDATE" SQL statement
    std::string sql =   "UPDATE savedFiles SET size=?, type=?, lastWriteTime=?, hash=? WHERE path=? AND username=? AND mac=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError::prepare);

    //bind parameters
    sqlite3_bind_int64(stmt,1,size);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,2,type.c_str(),type.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,3,lastWriteTime.c_str(),lastWriteTime.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,4,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,5,path.c_str(),path.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,6,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);
    sqlite3_bind_text(stmt,7,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError::prepare);

    //execute SQL statement
    rc = sqlite3_step(stmt);
    _handleSQLError(rc, SQLITE_DONE, "Cannot update row in savedFiles table: ", DatabaseError::update);

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method used to update an element of the database for a specified user-mac pair
 *
 * @param username username
 * @param mac mac address of the client host
 * @param d Directory_entry element to be updated
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::update(const std::string &username, const std::string &mac, Directory_entry &d) {
    std::string type;
    if(d.getType() == Directory_entry_TYPE::file)   //if the element is a file
        type = "file";
    else
        type = "directory";

    //update the element in the database
    update(username, mac, d.getRelativePath(), type, d.getSize(), d.getLastWriteTime(), d.getHash().str());
}