//
// Created by michele on 21/10/2020.
//

#include <fstream>
#include "Database.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Database class
 */

//static variable definition
std::shared_ptr<server::Database> server::Database::database_;
std::mutex server::Database::mutex_;

/**
 * Database class singleton instance getter
 *
 * @param path path of the database on disk
 * @return Database instance
 *
 * @throw DatabaseException in case of database errors (cannot open database or cannot create table)
 *
 * @author Michele Crepaldi s269551
 */
std::shared_ptr<server::Database> server::Database::getInstance(const std::string &path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(database_ == nullptr) //first time, or when it was released from everybody
        database_ = std::shared_ptr<Database>(new Database(path));  //create the database object
    return database_;
}

/**
 * private constructor for class Database
 *
 * @param path path of the database on disk
 *
 * @throw DatabaseException in case of database errors (cannot open database or cannot create table)
 *
 * @author Michele Crepaldi s269551
 */
server::Database::Database(std::string path) : path_(std::move(path)) {
    open(path_);
}

/**
 * utility function used to handle the SQL errors
 *
 * @param rc code returned from the SQLite function
 * @param check code to check the returned code against
 * @param message eventual error message to print
 * @param zErrMsg error string returned from the SQLite function
 * @param err databaseError to insert into the exception
 *
 * @throw DatabaseException in case rc != check
 *
 * @author Michele Crepaldi s269551
 */
void handleSQLError(int rc, int check, std::string &&message, char *zErrMsg, server::databaseError err){
    if(rc != check) {
        std::stringstream tmp;
        tmp << message << zErrMsg;
        sqlite3_free(zErrMsg);
        throw server::DatabaseException(tmp.str(), err);
    }
}

/**
 * function used to open the connection to a sqlite3 database; if the database already existed then it opens it, otherwise it also creates the needed table
 *
 * @param path path of the database in the filesystem
 *
 * @throw DatabaseException in case of database errors (cannot open database or cannot create table)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::open(const std::string &path) {
    int rc;
    //check if the db already exists before opening it (open will create a new db if none is found)
    bool create = !std::filesystem::directory_entry(path).exists();

    if(create){ //if the file does not exist create it
        std::ofstream f;
        std::filesystem::path p{path};
        auto parent = p.parent_path();  //get the parent path of the path
        std::filesystem::create_directories(parent);    //create all the directories (if they do not already exist) of the parent path
        f.open(path, std::ios::out | std::ios::trunc);  //create the file
        f.close();  //close the file
    }

    //open the database
    sqlite3 *dbTmp;
    rc = sqlite3_open(path_.c_str(), &dbTmp); //open the database
    db.reset(dbTmp);    //assign the newly opened db

    //in case of errors throw an exception
    if(rc) {
        std::stringstream tmp;
        tmp << "Cannot open database: " << sqlite3_errmsg(db.get());
        throw DatabaseException(tmp.str(), databaseError::open);
    }

    //if the db is new then create the table inside it
    if(create){
        char *zErrMsg = nullptr;
        //Create SQL statement
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
        rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);

        handleSQLError(rc, SQLITE_OK, "Cannot create table: ", zErrMsg, databaseError::create); //if there was an error throw an exception
    }
}

/**
 * function used to apply a function to each row of the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param f function to be used for each row extracted from the database
 *
 * @throw DatabaseException in case of database errors (cannot prepare, read or finalize)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::forAll(std::string &username, std::string &mac, std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> &f) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;
    char *zErrMsg = nullptr;
    //statement handle
    sqlite3_stmt* stmt;
    //Create SQL statement
    std::stringstream sql;
    sql << "SELECT path, size, type, lastWriteTime FROM savedFiles WHERE username=? AND mac=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare(db.get(), sql.str().c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //prepare some variables
    bool done = false;
    std::string path, type, lastWriteTime, hash, hashHex;
    uintmax_t size;

    //loop over table content
    while (!done) {
        switch (sqlite3_step (stmt)) {
            case SQLITE_ROW:    //in case of a row
                //convert columns
                path = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
                type = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
                size = sqlite3_column_int(stmt, 2);
                lastWriteTime = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)));
                hashHex = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
                hash = RandomNumberGenerator::hex_to_string(hashHex);

                //use function provided
                f(path, type, size, lastWriteTime, hash);
                break;

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;
                tmp << "Cannot read table: " << zErrMsg;
                sqlite3_free(zErrMsg);
                throw DatabaseException(tmp.str(), databaseError::read);
        }
    }

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, databaseError::finalize); //if there was an error throw an exception

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * function used to insert a new element in the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param path path of the element to be inserted
 * @param type type of the element to be inserted
 * @param size size of the element to be inserted
 * @param lastWriteTime last write time of the element to be inserted
 *
 * @throw DatabaseException in case of database errors (cannot prepare, insert or finalize)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::insert(std::string &username, std::string &mac, const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;
    char *zErrMsg = nullptr;
    std::string hashHex = RandomNumberGenerator::string_to_hex(hash);   //convert hash to hexadecimal representation (to store it)

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "INSERT INTO savedFiles (username, mac, path, size, type, lastWriteTime, hash) VALUES (?,?,?,?,?,?,?);";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,path.c_str(),path.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,4,type.c_str(),type.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_int64(stmt,5,size);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,6,lastWriteTime.c_str(),lastWriteTime.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,7,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot add user to password table: ", zErrMsg, databaseError::insert); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, databaseError::finalize); //if there was an error throw an exception

    sqlite3_finalize(stmt);
}

/**
 * function used to insert a new element in the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param d element to be inserted
 *
 * @throw DatabaseException in case of database errors (cannot prepare, insert or finalize)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::insert(std::string &username, std::string &mac, Directory_entry &d) {
    std::string type;
    if(d.getType() == Directory_entry_TYPE::file)
        type = "file";
    else
        type = "directory";
    insert(username, mac, d.getRelativePath(), type, d.getSize(), d.getLastWriteTime(), d.getHash().str());
}

/**
 * function used to remove an element from the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param path path of the element to be removed
 *
 * @throw DatabaseException in case of database errors (cannot prepare, remove or finalize)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::remove(std::string &username, std::string &mac, const std::string &path) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;
    char *zErrMsg = nullptr;

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "DELETE FROM savedFiles WHERE path=? AND username=? AND mac=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //bind parameter
    sqlite3_bind_text(stmt,1,path.c_str(),path.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot remove user from password table: ", zErrMsg, databaseError::remove); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, databaseError::finalize); //if there was an error throw an exception

    sqlite3_finalize(stmt);
}

/**
 * function used to update an element of the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param path path of the element to be update
 * @param type type of the element to be update
 * @param size size of the element to be update
 * @param lastWriteTime last write time of the element to be update
 *
 * @throw DatabaseException in case of database errors (cannot prepare, update or finalize)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::update(std::string &username, std::string &mac, const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;
    char *zErrMsg = nullptr;
    std::string hashHex = RandomNumberGenerator::string_to_hex(hash);   //convert hash to hexadecimal representation (to store it)

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "UPDATE savedFiles SET size=?, type=?, lastWriteTime=?, hash=?, WHERE path=? AND username=? AND mac=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,mac.c_str(),mac.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,path.c_str(),path.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,4,type.c_str(),type.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_int64(stmt,5,size);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,6,lastWriteTime.c_str(),lastWriteTime.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,7,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot remove user from password table: ", zErrMsg, databaseError::update); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, databaseError::finalize); //if there was an error throw an exception

    sqlite3_finalize(stmt);
}

/**
 * function used to update an element of the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param d element to be update
 *
 * @throw DatabaseException in case of database errors (cannot prepare, update or finalize)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::update(std::string &username, std::string &mac, Directory_entry &d) {
    std::string type;
    if(d.getType() == Directory_entry_TYPE::file)
        type = "file";
    else
        type = "directory";
    update(username, mac, d.getRelativePath(), type, d.getSize(), d.getLastWriteTime(), d.getHash().str());
}