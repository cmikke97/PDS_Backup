//
// Created by Michele Crepaldi s269551 on 21/10/2020
// Finished on 20/11/2020
// Last checked on 20/11/2020
//

#include "Database_pwd.h"

#include <filesystem>
#include <fstream>

#include "../myLibraries/RandomNumberGenerator.h"


#define SALT_SIZE 32

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Password Database class
 */

//static variable definition
std::shared_ptr<server::Database_pwd> server::Database_pwd::database_;
std::mutex server::Database_pwd::mutex_;
std::string server::Database_pwd::path_;

/**
 * Database_pwd class path_ variable setter
 *
 * @param path path of the database on disk
 *
 * @author Michele Crepaldi s269551
 */
void server::Database_pwd::setPath(std::string path){
    path_ = std::move(path);    //set the path_
}

/**
 * Database_pwd class singleton instance getter method
 *
 * @return PWD_Database instance
 *
 * @author Michele Crepaldi s269551
 */
std::shared_ptr<server::Database_pwd> server::Database_pwd::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(database_ == nullptr) //first time, or when it was released from everybody
        database_ = std::shared_ptr<Database_pwd>(new Database_pwd());  //create the database object
    return database_;
}

/**
 * (protected) constructor of the Database_pwd object
 *
 * @throws DatabaseException_pwd:
 *  <b>path</b> if no path was set before this call
 *
 * @author Michele Crepaldi s269551
 */
server::Database_pwd::Database_pwd(){
    _open();
}

/**
 * method used to handle the SQL errors
 *
 * @param rc code returned from the SQLite function
 * @param check SQLite code to check the returned code against
 * @param message eventual error message to print (moved)
 * @param err databaseError_pwd to insert into the possible exception
 *
 * @throws DatabaseException_pwd:
 *  <b>[err]</b> in case rc != check
 *
 * @author Michele Crepaldi s269551
 */
void server::Database_pwd::_handleSQLError(int rc, int check, std::string &&message, DatabaseError_pwd err){
    if(rc != check) {   //if there was an error
        std::stringstream tmp;

        //get the error message also from the sqlite3 object
        tmp << message << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(_db.get());

        throw server::DatabaseException_pwd(tmp.str(), err);    //throw exception
    }
}

/**
 * method used to open the connection to a sqlite3 database; if the database already exists then it opens it,
 *  otherwise it also creates the needed table
 *
 * @throws DatabaseException_pwd:
 *  <b>open</b> if the database could not be opened
 * @throws DatabaseException_pwd:
 *  <b>create</b> if the (new) database could not be created
 *
 * @author Michele Crepaldi s269551
 */
void server::Database_pwd::_open() {
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
    rc = sqlite3_open(path_.c_str(), &dbTmp); //open the database
    _db.reset(dbTmp);   //assign the newly opened db to the _db smart pointer

    _handleSQLError(rc, SQLITE_OK, "Cannot open database: ", DatabaseError_pwd::open);

    //if the db is new then create the table inside it
    if(!dbExists){
        //"CREATE" SQL statement
        std::string sql = "CREATE TABLE passwords("
                          "id INTEGER,"
                          "username TEXT UNIQUE,"
                          "salt TEXT,"
                          "hash TEXT,"
                          "PRIMARY KEY(id AUTOINCREMENT));";

        //Execute SQL statement
        rc = sqlite3_exec(_db.get(), sql.c_str(), nullptr, nullptr, nullptr);

        _handleSQLError(rc, SQLITE_OK, "Cannot create table: ", DatabaseError_pwd::create);
    }
}

/**
 * method used to get the salt and hash associated to the specified user
 *
 * @param username username
 *
 * @return pair with salt (string) and hash (Hash)
 *
 * @throws DatabaseException_pwd:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException_pwd:
 *  <b>read</b> if the database could not be read (ho hash could be retrieved)
 * @throws DatabaseException_pwd:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
std::pair<std::string, Hash> server::Database_pwd::getHash(const std::string &username) {
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code
    sqlite3_stmt* stmt; //statement handle

    std::string saltHex;    //hex representation of the salt
    std::string hashHex;    //hex representation of the hash

    //"SELECT" SQL statement
    std::string sql = "SELECT salt, hash FROM passwords WHERE username=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError_pwd::prepare);

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError_pwd::prepare);

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare);

    bool done = false;
    //loop over table content
    while (!done) {
        switch (rc = sqlite3_step (stmt)) { //execute a step of the sql statement on the database
            case SQLITE_ROW:    //in case a database row was extracted

                //get column values from the row (and convert them)

                saltHex = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
                hashHex = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));

                done = true;    //since usernames are unique exit from loop
                break;

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;

                //get the error message also from the sqlite3 object
                tmp << "Cannot read table: " << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(_db.get());

                throw DatabaseException_pwd(tmp.str(), DatabaseError_pwd::read);
        }
    }

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError_pwd::finalize);

    //finalize statement handle
    sqlite3_finalize(stmt);

    if(saltHex.empty() || hashHex.empty())  //if they are empty it means no user with the provided username was registered
        return std::make_pair("",Hash());   //return an empty pair
        
    //convert hash from hex representation (as it is stored in the database)
    //to bitstring representation (as it is used in the program)

    std::string salt = RandomNumberGenerator::hex_to_string(saltHex);   //bitstring representation of the salt
    std::string hash = RandomNumberGenerator::hex_to_string(hashHex);   //bitstring representation of the hash
    return std::make_pair(salt, Hash(hash));    //return the salt and hash
}

/**
 * method to add a user into the password database
 *
 * @param username username
 * @param password password
 *
 * @throws DatabaseException_pwd:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException_pwd:
 *  <b>insert</b> if the row could not be inserted into the database
 * @throws DatabaseException_pwd:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database_pwd::addUser(const std::string &username, const std::string &password){
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code

    RandomNumberGenerator rng;  //random number generator

    std::string salt = rng.getRandomString(SALT_SIZE);  //randomly generated salt

    HashMaker hm{password}; //hash maker (initialized with the password)
    hm.update(salt);    //update the hash maker with the randomly generated salt

    //convert hash from bitstring representation (as it is used in the program)
    //to hex representation (as it is stored in the database)

    std::string saltHex = RandomNumberGenerator::string_to_hex(salt);   //hex representation of the salt
    std::string hashHex = RandomNumberGenerator::string_to_hex(hm.get().str()); //hex representation of the hash

    sqlite3_stmt* stmt; //statement handle

    //"INSERT" SQL statement
    std::string sql =   "INSERT INTO passwords (username, salt, hash) VALUES (?,?,?);";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,saltHex.c_str(),saltHex.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //execute SQL statement
    rc = sqlite3_step(stmt);
    _handleSQLError(rc, SQLITE_DONE, "Cannot insert user: ", DatabaseError_pwd::insert); //if there was an error throw an exception

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError_pwd::finalize); //if there was an error throw an exception

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method to update a user's password
 *
 * @param username username
 * @param password new password
 *
 * @throws DatabaseException_pwd:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException_pwd:
 *  <b>update</b> if the row could not be updated in the database
 * @throws DatabaseException_pwd:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database_pwd::updateUser(const std::string &username, const std::string &password){
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code

    RandomNumberGenerator rng;  //random number generator

    std::string salt = rng.getRandomString(SALT_SIZE);  //randomly generated salt

    HashMaker hm{password}; //hash maker (initialized with the password)
    hm.update(salt);    //update the hash maker with the randomly generated salt

    //convert hash from bitstring representation (as it is used in the program)
    //to hex representation (as it is stored in the database)

    std::string saltHex = RandomNumberGenerator::string_to_hex(salt);   //hex representation of the salt
    std::string hashHex = RandomNumberGenerator::string_to_hex(hm.get().str()); //hex representation of the hash

    sqlite3_stmt* stmt; //statement handle

    //"UPDATE" SQL statement
    std::string sql =   "UPDATE passwords SET salt=?, hash=? WHERE username=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,saltHex.c_str(),saltHex.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //execute SQL statement
    rc = sqlite3_step(stmt);
    _handleSQLError(rc, SQLITE_DONE, "Cannot update user: ", DatabaseError_pwd::update); //if there was an error throw an exception

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError_pwd::finalize); //if there was an error throw an exception

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method to remove a user from the password database
 *
 * @param username username
 *
 * @throws DatabaseException_pwd:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException_pwd:
 *  <b>remove</b> if the row could not be removed from the database
 * @throws DatabaseException_pwd:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database_pwd::removeUser(const std::string &username){
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc;  //sqlite3 methods' return code

    sqlite3_stmt* stmt;  //statement handle

    //"DELETE" SQL statement
    std::string sql =   "DELETE FROM passwords WHERE username=?;";

    //prepare SQL statement
    rc = sqlite3_prepare_v2(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //bind parameter
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    _handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //execute SQL statement
    rc = sqlite3_step(stmt);
    _handleSQLError(rc, SQLITE_DONE, "Cannot remove user: ", DatabaseError_pwd::remove); //if there was an error throw an exception

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError_pwd::finalize); //if there was an error throw an exception

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * method used to apply a provided function to each row of the database
 *
 * @param f function to be used for each row extracted from the database
 *
 * @throws DatabaseException_pwd:
 *  <b>prepare</b> if the sql statement could not be prepared (or there is an error in some parameter binding)
 * @throws DatabaseException_pwd:
 *  <b>read</b> if the database could not be read
 * @throws DatabaseException_pwd:
 *  <b>finalize</b> if the sql statement could not be finalized
 *
 * @author Michele Crepaldi s269551
 */
void server::Database_pwd::forAll(std::function<void (const std::string &)> &f) {
    std::lock_guard<std::mutex> lock(_access_mutex);    //lock guard on _access_mutex to ensure thread safeness

    int rc; //sqlite3 methods' return code
    sqlite3_stmt* stmt; //statement handle

    //"SELECT" SQL statement
    std::string sql = "SELECT username FROM passwords;";

    //prepare SQL statement
    rc = sqlite3_prepare(_db.get(), sql.c_str(), -1, &stmt, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", DatabaseError_pwd::prepare); //if there was an error throw an exception

    //begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(_db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", DatabaseError_pwd::prepare); //if there was an error throw an exception


    bool done = false;
    //loop over table content
    while (!done) {
        switch (rc = sqlite3_step (stmt)) { //execute a step of the sql statement on the database
            case SQLITE_ROW:    //in case a database row was extracted
            {
                //get column values from the row (and convert them)

                //username
                std::string username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));

                //use provided function
                f(username);
                break;
            }

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;

                //get the error message also from the sqlite3 object
                tmp << "Cannot read table: " << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(_db.get());

                throw DatabaseException_pwd(tmp.str(), DatabaseError_pwd::read);
        }
    }

    //end the transaction
    rc = sqlite3_exec(_db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    _handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", DatabaseError_pwd::finalize); //if there was an error throw an exception

    //finalize statement handle
    sqlite3_finalize(stmt);
}

