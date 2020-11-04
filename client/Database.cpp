//
// Created by michele on 28/09/2020.
//

#include <fstream>
#include "Database.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Database class
 */

//static variable definition
std::shared_ptr<client::Database> client::Database::database_;
std::mutex client::Database::mutex_;

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
std::shared_ptr<client::Database> client::Database::getInstance(const std::string &path) {
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
client::Database::Database(std::string path) : path_(std::move(path)) {
    open(path_);
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
void client::Database::open(const std::string &path) {
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
    rc = sqlite3_open(path_.c_str(), &dbTmp);   //open the database
    db.reset(dbTmp);    //assign it to the current db pointer

    handleSQLError(rc, SQLITE_OK, "Cannot open database: ", databaseError::open); //if there was an error throw an exception

    //if the db is new then create the table inside it
    if(create){
        //Create SQL statement
        std::string sql = "CREATE TABLE savedFiles("
                          "id INTEGER,"
                          "path TEXT UNIQUE,"
                          "size INTEGER,"
                          "type TEXT,"
                          "lastWriteTime TEXT,"
                          "hash TEXT,"
                          "PRIMARY KEY(id AUTOINCREMENT));";

        //Execute SQL statement
        rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, nullptr);

        handleSQLError(rc, SQLITE_OK, "Cannot create table: ", databaseError::create); //if there was an error throw an exception
    }
}

/**
 * utility function used to handle the SQL errors
 *
 * @param rc code returned from the SQLite function
 * @param check code to check the returned code against
 * @param message eventual error message to print
 * @param err databaseError to insert into the exception
 *
 * @throw DatabaseException in case rc != check
 *
 * @author Michele Crepaldi s269551
 */
void client::Database::handleSQLError(int rc, int check, std::string &&message, databaseError err){
    if(rc != check) {
        std::stringstream tmp;
        tmp << message << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(db.get());
        throw client::DatabaseException(tmp.str(), err);
    }
}

/**
 * function used to apply a function to each row of the database
 *
 * @param f function to be used for each row extracted from the database
 *
 * @throw DatabaseException in case of database errors (cannot prepare or read)
 *
 * @author Michele Crepaldi s269551
 */
void client::Database::forAll(std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> &f) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;
    //statement handle
    sqlite3_stmt* stmt;
    //Create SQL statement
    std::string sql = "SELECT path, type, size, lastWriteTime, hash from savedFiles;";

    //Prepare SQL statement
    rc = sqlite3_prepare(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare table: ", databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", databaseError::prepare); //if there was an error throw an exception

    //prepare some variables
    bool done = false;
    std::string path, type, lastWriteTime, hash, hashHex;
    uintmax_t size;

    //loop over table content
    while (!done) {
        switch (rc = sqlite3_step (stmt)) {
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
                tmp << "Cannot read table: " << sqlite3_errstr(rc) << "; " << sqlite3_errmsg(db.get());
                throw DatabaseException(tmp.str(), databaseError::read);
        }
    }

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", databaseError::finalize); //if there was an error throw an exception

    //finalize statement handle
    sqlite3_finalize(stmt);
}

/**
 * simple function used to properly quote values for the SQL statement
 *
 * @param s string to be quoted
 * @return computed string
 *
 * @author Michele Crepaldi s269551
 */
std::string quotesql( const std::string& s ) {
    return std::string("'") + s + std::string("'");
}

/**
 * function used to insert a new element in the database
 *
 * @param path path of the element to be inserted
 * @param type type of the element to be inserted
 * @param size size of the element to be inserted
 * @param lastWriteTime last write time of the element to be inserted
 *
 * @throw DatabaseException in case of database errors (cannot insert)
 *
 * @author Michele Crepaldi s269551
 */
void client::Database::insert(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;
    std::string hashHex = RandomNumberGenerator::string_to_hex(hash);   //convert hash to hexadecimal representation (to store it)

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "INSERT INTO savedFiles (path, type, size, lastWriteTime, hash) VALUES (?,?,?,?,?);";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_text(stmt,1,path.c_str(),path.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,type.c_str(),type.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_int64(stmt,3,size);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,4,lastWriteTime.c_str(),lastWriteTime.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,5,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot insert into table: ", databaseError::insert); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", databaseError::finalize); //if there was an error throw an exception

    sqlite3_finalize(stmt);
}

/**
 * function used to insert a new element in the database
 *
 * @param d element to be inserted
 *
 * @throw DatabaseException in case of database errors (cannot insert)
 *
 * @author Michele Crepaldi s269551
 */
void client::Database::insert(Directory_entry &d) {
    std::string type;
    if(d.getType() == Directory_entry_TYPE::file)
        type = "file";
    else
        type = "directory";
    insert(d.getRelativePath(), type, d.getSize(), d.getLastWriteTime(), d.getHash().str());
}

/**
 * function used to remove an element from the database
 *
 * @param path path of the element to be removed
 *
 * @throw DatabaseException in case of database errors (cannot delete)
 *
 * @author Michele Crepaldi s269551
 */
void client::Database::remove(const std::string &path) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "DELETE FROM savedFiles WHERE path=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", databaseError::prepare); //if there was an error throw an exception

    //bind parameter
    sqlite3_bind_text(stmt,1,path.c_str(),path.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot delete from table: ", databaseError::remove); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", databaseError::finalize); //if there was an error throw an exception

    sqlite3_finalize(stmt);
}

/**
 * function used to update an element of the database
 *
 * @param path path of the element to be update
 * @param type type of the element to be update
 * @param size size of the element to be update
 * @param lastWriteTime last write time of the element to be update
 *
 * @throw DatabaseException in case of database errors (cannot update)
 *
 * @author Michele Crepaldi s269551
 */
void client::Database::update(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash) {
    std::lock_guard<std::mutex> lock(access_mutex); //ensure thread safeness

    int rc;
    std::string hashHex = RandomNumberGenerator::string_to_hex(hash);   //convert hash to hexadecimal representation (to store it)

    //statement handle
    sqlite3_stmt* stmt;

    //Create SQL statement
    std::string sql =   "UPDATE savedFiles SET size=?, type=?, lastWriteTime=?, hash=? WHERE path=?;";

    //Prepare SQL statement
    rc = sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot prepare SQL statement: ", databaseError::prepare); //if there was an error throw an exception

    //Begin the transaction (will most likely increase performance)
    rc = sqlite3_exec(db.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot begin transaction: ", databaseError::prepare); //if there was an error throw an exception

    //bind parameters
    sqlite3_bind_int64(stmt,1,size);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,2,type.c_str(),type.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,3,lastWriteTime.c_str(),lastWriteTime.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,4,hashHex.c_str(),hashHex.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception
    sqlite3_bind_text(stmt,5,path.c_str(),path.length(),SQLITE_TRANSIENT);
    handleSQLError(rc, SQLITE_OK, "Cannot bind the parameters: ", databaseError::prepare); //if there was an error throw an exception

    // Execute SQL statement
    rc = sqlite3_step(stmt);
    handleSQLError(rc, SQLITE_DONE, "Cannot update value in table: ", databaseError::update); //if there was an error throw an exception

    //End the transaction.
    rc = sqlite3_exec(db.get(), "END TRANSACTION", nullptr, nullptr, nullptr);
    handleSQLError(rc, SQLITE_OK, "Cannot end the transaction: ", databaseError::finalize); //if there was an error throw an exception

    sqlite3_finalize(stmt);
}

/**
 * function used to update an element of the database
 *
 * @param d element to be update
 *
 * @throw DatabaseException in case of database errors (cannot update)
 *
 * @author Michele Crepaldi s269551
 */
void client::Database::update(Directory_entry &d) {
    std::string type;
    if(d.getType() == Directory_entry_TYPE::file)
        type = "file";
    else
        type = "directory";
    update(d.getRelativePath(), type, d.getSize(), d.getLastWriteTime(), d.getHash().str());
}