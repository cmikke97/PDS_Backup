//
// Created by michele on 21/10/2020.
//

#include "PWD_Database.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Password Database class
 */

//static variable definition
std::weak_ptr<server::PWD_Database> server::PWD_Database::database_;
std::mutex server::PWD_Database::mutex_;

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
std::shared_ptr<server::PWD_Database> server::PWD_Database::getInstance(const std::string &path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(database_.expired()) //first time, or when it was released from everybody
        database_ = std::shared_ptr<PWD_Database>(new PWD_Database(path));  //create the database object
    return database_.lock();
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
server::PWD_Database::PWD_Database(std::string path) : path_(std::move(path)) {
    open(path_);
}

/**
 * function used in select count(*) statement to get the result
 *
 * @param count count to be returned
 * @param argc number of db row elements
 * @param argv db row elements
 * @param azColName names of the row elements
 * @return callback error code
 *
 * @author Michele Crepaldi s269551
 */
static int countCallback(void *count, int argc, char **argv, char **azColName) {
    int *c = static_cast<int *>(count);
    *c = std::stoi(argv[0]);
    return 0;
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
void server::PWD_Database::open(const std::string &path) {
    int rc;
    //check if the db already exists before opening it (open will create a new db if none is found)
    bool create = !std::filesystem::directory_entry(path).exists();

    //open the database
    sqlite3 *dbTmp;
    rc = sqlite3_open(path_.c_str(), &dbTmp);

    //db = mypwdb::UniquePtr<sqlite3>(dbTmp);
    db.reset(dbTmp);    //assign the newly opened db

    //in case of errors throw an exception
    if(rc) {
        std::stringstream tmp;
        tmp << "Cannot open database: " << sqlite3_errmsg(db.get());
        throw PWT_DatabaseException(tmp.str(), pwt_databaseError::open);
    }

    //if the db is new then create the table inside it and then exit (there are no users)
    if(create){
        char *zErrMsg = nullptr;
        //Create SQL statement
        std::string sql = "CREATE TABLE \"passwords\" (/"
                          "\"id\" INTEGER,/"
                          "\"username\" TEXT UNIQUE,/"
                          "\"salt\" TEXT,/"
                          "\"hash\" TEXT,/"
                          "PRIMARY KEY(\"id\" AUTOINCREMENT)/"
                          ");";

        //Execute SQL statement
        rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);

        //if there was an error in the table creation throw an exception
        if( rc != SQLITE_OK ){
            std::stringstream tmp;
            tmp << "Cannot create table: " << zErrMsg;
            sqlite3_free(zErrMsg);
            throw PWT_DatabaseException(tmp.str(), pwt_databaseError::create);
        }

        //exit since there are no users
        throw PWT_DatabaseException("Password db did not exist, a new one has been created;/"
                                "insert users in it and start the program again", pwt_databaseError::create);
    }

    //check if there is at least one user
    int count;
    char *zErrMsg = nullptr;
    //Create SQL statement
    std::string sql = "SELECT COUNT(*) FROM passwords";
    //Execute SQL statement
    rc = sqlite3_exec(db.get(), sql.c_str(), countCallback, &count, &zErrMsg);
    //in case of errors throw an exception
    if(rc) {
        std::stringstream tmp;
        tmp << "Error in database read: " << sqlite3_errmsg(db.get());
        throw PWT_DatabaseException(tmp.str(), pwt_databaseError::read);
    }
    //if there are no users then throw error and exit
    if(count == 0)
        throw PWT_DatabaseException("There are no users in the database; please insert some.", pwt_databaseError::read);
}

/**
 * function used in select salt and hash statement to get the result
 *
 * @param pair pair to be returned
 * @param argc number of db row elements
 * @param argv db row elements
 * @param azColName names of the row elements
 * @return callback error code
 *
 * @throw HashException in case the hash retrieved is of wrong size
 *
 * @author Michele Crepaldi s269551
 */
static int hashCallback(void *pair, int argc, char **argv, char **azColName) {
    auto *c = static_cast<std::pair<std::string, Hash> *>(pair);
    *c = std::make_pair(argv[0], Hash(argv[1]));
    return 0;
}

/**
 * function used to get the salt and hash associated to a user
 *
 * @param username username of the user to retrieve info of
 * @return pair with salt and hash
 *
 * @throw DatabaseException in case it cannot read from db
 * @throw HashException in case the hash retrieved is of wrong size
 *
 * @author Michele Crepaldi s269551
 */
std::pair<std::string, Hash> server::PWD_Database::getHash(std::string &username) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    char *zErrMsg = nullptr;
    //Create SQL statement
    std::stringstream sql;
    sql << "SELECT salt, hash FROM passwords WHERE username=" << username;

    //prepare the variable to collect info into and return
    std::pair<std::string, Hash> hashPair;

    //Execute SQL statement
    int rc = sqlite3_exec(db.get(), sql.str().c_str(), hashCallback, &hashPair, &zErrMsg);
    //in case of errors throw an exception
    if(rc) {
        std::stringstream tmp;
        tmp << "Error in database read: " << sqlite3_errmsg(db.get());
        throw PWT_DatabaseException(tmp.str(), pwt_databaseError::read);
    }

    return hashPair;
}

