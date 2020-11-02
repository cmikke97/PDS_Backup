//
// Created by michele on 21/10/2020.
//

#include <fstream>
#include "PWD_Database.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Password Database class
 */

//static variable definition
std::shared_ptr<server::PWD_Database> server::PWD_Database::database_;
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
    if(database_ == nullptr) //first time, or when it was released from everybody
        database_ = std::shared_ptr<PWD_Database>(new PWD_Database(path));  //create the database object
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

    if(create){ //if the file does not exist create it
        std::ofstream f;
        std::filesystem::path p{path};
        auto parent = p.parent_path();  //get the parent path of the path
        std::filesystem::create_directories(parent);    //create all the directories (if they do not already exist) of the parent path
        f.open(path, std::ios::out | std::ios::trunc);  //create the file
        f.close();  //close the file
    }

    sqlite3 *dbTmp;
    rc = sqlite3_open(path_.c_str(), &dbTmp); //open the database
    db.reset(dbTmp);    //assign the newly opened db

    //in case of errors throw an exception
    if(rc) {
        std::stringstream tmp;
        tmp << "Cannot open database: " << sqlite3_errmsg(db.get());
        throw PWT_DatabaseException(tmp.str(), PWT_databaseError::open);
    }

    //if the db is new then create the table inside it and then exit (there are no users)
    if(create){
        char *zErrMsg = nullptr;
        //Create SQL statement
        std::string sql = "CREATE TABLE passwords("
                          "id INTEGER,"
                          "username TEXT UNIQUE,"
                          "salt TEXT,"
                          "hash TEXT,"
                          "PRIMARY KEY(id AUTOINCREMENT));";

        //Execute SQL statement
        rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);

        //if there was an error in the table creation throw an exception
        if( rc != SQLITE_OK ){
            std::stringstream tmp;
            tmp << "Cannot create table: " << zErrMsg;
            sqlite3_free(zErrMsg);
            throw PWT_DatabaseException(tmp.str(), PWT_databaseError::create);
        }

        //exit since there are no users
        //throw PWT_DatabaseException("Password db did not exist, a new one has been created; insert users in it and start the program again", PWT_databaseError::create);
    }

    /*
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
        throw PWT_DatabaseException(tmp.str(), PWT_databaseError::read);
    }
    //if there are no users then throw error and exit
    if(count == 0)
        throw PWT_DatabaseException("There are no users in the database; please insert some.", PWT_databaseError::read);
    */
}

/**
 * utility function used to handel the SQL errors
 *
 * @param rc code returned from the SQLite function
 * @param check code to check the returned code against
 * @param message eventual error message to print
 * @param zErrMsg error string returned from the SQLite function
 * @param err PWT_databaseError to insert into the exception
 *
 * @throw PWT_DatabaseException in case rc != check
 *
 * @author Michele Crepaldi s269551
 */
void handleSQLError(int rc, int check, std::string &&message, char *zErrMsg, server::PWT_databaseError err){
    if(rc != check) {
        std::stringstream tmp;
        tmp << message << zErrMsg;
        sqlite3_free(zErrMsg);
        throw server::PWT_DatabaseException(tmp.str(), err);
    }
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
    std::string tmp1 = std::string(argv[0]);
    std::string tmp2 = std::string(argv[1]);
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
    int rc;

    //TODO convert from hex (in db) to byte string the salt and the hash

    std::string salt, hash;

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql = "SELECT salt, hash FROM passwords WHERE username=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    bool done = false;
    //loop over table content
    while (!done) {
        switch (sqlite3_step (stmt)) {
            case SQLITE_ROW:    //in case of a row
                //convert columns
                salt = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
                hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
                break;

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;
                tmp << "Cannot read table: " << zErrMsg;
                sqlite3_free(zErrMsg);
                throw PWT_DatabaseException(tmp.str(), PWT_databaseError::hash);
        }
    }

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, PWT_databaseError::hash); //if there was an error throw an exception

    sqlite3_finalize(stmt);

    /*
    //prepare the variable to collect info into and return
    std::pair<std::string, Hash> hashPair;
    //Execute SQL statement
    int rc = sqlite3_exec(db.get(), sql.c_str(), hashCallback, &hashPair, &zErrMsg);
    //optionally sqlite3_errmsg(db.get()) instead of zErrMsg
    handleSQLError(rc, SQLITE_OK, "Cannot read from database: ", zErrMsg, PWT_databaseError::read); //if there was an error throw an exception
    return hashPair;
    */
    return std::make_pair(salt, Hash(hash));
}

/**
 * function to add a user into the password database
 *
 * @param username username of the user to insert
 * @param password password to associate to the user
 *
 * @throw PWT_DatabaseException in case the user cannot be inserted
 *
 * @author Michele Crepaldi s269551
 */
void server::PWD_Database::addUser(const std::string &username, const std::string &password){
    int rc;
    char *zErrMsg = nullptr;
    RandomNumberGenerator rng;
    std::string salt = rng.getRandomString(32); //TODO salt size as parameter (config)
    HashMaker hm{password};
    hm.update(salt);
    Hash passwordHash = hm.get();

    //TODO convert from byte string to hex the salt and the hash before putting them in the database

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "INSERT INTO passwords (username, salt, hash) VALUES (?,?,?);";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,salt.c_str(),salt.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,passwordHash.str().c_str(),passwordHash.str().length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot remove user from password table: ", zErrMsg, PWT_databaseError::remove); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, PWT_databaseError::remove); //if there was an error throw an exception

    sqlite3_finalize(stmt);

    /*
    // Execute SQL statement
    rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot insert into password table: ", zErrMsg, PWT_databaseError::insert); //if there was an error throw an exception
    */
}

/**
 * function to add a user into the password database
 *
 * @param username username of the user to insert
 * @param password password to associate to the user
 *
 * @throw PWT_DatabaseException in case the user cannot be inserted
 *
 * @author Michele Crepaldi s269551
 */
void server::PWD_Database::updateUser(const std::string &username, const std::string &password){
    int rc;
    char *zErrMsg = nullptr;
    RandomNumberGenerator rng;
    std::string salt = rng.getRandomString(32); //TODO salt size as parameter (config)
    HashMaker hm{password};
    hm.update(salt);
    Hash passwordHash = hm.get();

    //TODO convert from byte string to hex the salt and the hash before putting them in the database

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "UPDATE passwords SET salt=?, hash=? WHERE username=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,salt.c_str(),salt.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,passwordHash.str().c_str(),passwordHash.str().length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot remove user from password table: ", zErrMsg, PWT_databaseError::remove); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, PWT_databaseError::remove); //if there was an error throw an exception

    sqlite3_finalize(stmt);

    /*
    // Execute SQL statement
    rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot update username into password table: ", zErrMsg, PWT_databaseError::update); //if there was an error throw an exception
    */
}

/**
 * function to add a user into the password database
 *
 * @param username username of the user to insert
 * @param password password to associate to the user
 *
 * @throw PWT_DatabaseException in case the user cannot be inserted
 *
 * @author Michele Crepaldi s269551
 */
void server::PWD_Database::removeUser(const std::string &username){
    int rc;
    char *zErrMsg = nullptr;
    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "DELETE FROM passwords WHERE username=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //bind parameter
    sqlite3_bind_text(stmt,1,username.c_str(),username.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot remove user from password table: ", zErrMsg, PWT_databaseError::remove); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, PWT_databaseError::remove); //if there was an error throw an exception

    sqlite3_finalize(stmt);
}

/**
 * function used to apply a function to each row of the database
 *
 * @param f function to be used for each row extracted from the database
 *
 * @throw PWT_DatabaseException in case of database errors (cannot prepare or read)
 *
 * @author Michele Crepaldi s269551
 */
void server::PWD_Database::forAll(std::function<void (const std::string &)> &f) {
    int rc;
    char *zErrMsg = nullptr;
    //statement handle
    sqlite3_stmt* stmt;
    //Create SQL statement
    std::stringstream sql;
    sql << "SELECT username FROM passwords;";

    //Prepare SQL statement
    rc = sqlite3_prepare(db.get(), sql.str().c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", zErrMsg, PWT_databaseError::prepare); //if there was an error throw an exception

    //prepare some variables
    bool done = false;
    std::string username;

    //loop over table content
    while (!done) {
        switch (sqlite3_step (stmt)) {
            case SQLITE_ROW:    //in case of a row
                //convert columns
                username = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));

                //use function provided
                f(username);
                break;

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;
                tmp << "Cannot read table: " << zErrMsg;
                sqlite3_free(zErrMsg);
                throw PWT_DatabaseException(tmp.str(), PWT_databaseError::read);
        }
    }

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, &zErrMsg);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", zErrMsg, PWT_databaseError::remove); //if there was an error throw an exception

    //finalize statement handle
    sqlite3_finalize(stmt);
}

