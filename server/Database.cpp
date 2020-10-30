//
// Created by michele on 21/10/2020.
//

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

    //open the database
    sqlite3 *dbTmp;
    rc = sqlite3_open(path_.c_str(), &dbTmp);

    //db = mydb::UniquePtr<sqlite3>(dbTmp);
    db.reset(dbTmp);

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
        std::string sql = "CREATE TABLE \"savedFiles\" (/"
                          "\"id\" INTEGER,/"
                          "\"username\" TEXT,/"
                          "\"mac\" TEXT,/"
                          "\"path\" TEXT UNIQUE,/"
                          "\"size\" INTEGER,/"
                          "\"type\" TEXT,/"
                          "\"lastWriteTime\" TEXT,/"
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
            throw DatabaseException(tmp.str(), databaseError::create);
        }
    }
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
 * function used to apply a function to each row of the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param f function to be used for each row extracted from the database
 *
 * @throw DatabaseException in case of database errors (cannot prepare or read)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::forAll(std::string &username, std::string &mac, std::function<void (const std::string &, const std::string &, uintmax_t, const std::string &, const std::string &)> &f) {
    int rc;
    char *zErrMsg = nullptr;
    //statement handle
    sqlite3_stmt* stmt;
    //Create SQL statement
    std::stringstream sql;
    sql << "SELECT path, size, type, lastWriteTime FROM savedFiles WHERE username=";
    sql << quotesql(username) << " AND mac=" << quotesql(mac) << ";";

    //Prepare SQL statement
    rc = sqlite3_prepare(db.get(), sql.str().c_str(), -1, &stmt, nullptr);

    //if there was an error in the SQL statement prepare throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot prepare table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw DatabaseException(tmp.str(), databaseError::prepare);
    }

    //prepare some variables
    bool done = false;
    std::string path, type, lastWriteTime, hash;
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
                hash = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));

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
 * @throw DatabaseException in case of database errors (cannot insert)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::insert(std::string &username, std::string &mac, const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash) {
    int rc;
    char *zErrMsg = nullptr;
    //Create SQL statement
    std::string sql =   "INSERT INTO savedFiles (username, mac, path, size, type, lastWriteTime, hash) VALUES ("
                        + quotesql(username) + ","
                        + quotesql(mac) + ","
                        + quotesql(path) + ","
                        + quotesql(std::to_string(size)) + ","
                        + quotesql(type) + ","
                        + quotesql(lastWriteTime) + ","
                        + quotesql(hash) + ","
                        + ");";

    // Execute SQL statement
    rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);

    //if there was an error in the table insertion throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot insert into table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw DatabaseException(tmp.str(), databaseError::insert);
    }
}

/**
 * function used to insert a new element in the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param d element to be inserted
 *
 * @throw DatabaseException in case of database errors (cannot insert)
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
 * @throw DatabaseException in case of database errors (cannot delete)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::remove(std::string &username, std::string &mac, const std::string &path) {
    int rc;
    char *zErrMsg = nullptr;
    //Create SQL statement
    std::string sql =   "DELETE FROM savedFiles WHERE path="
                        + quotesql(path) + " AND username="
                        + quotesql(username) + "AND mac="
                        + quotesql(mac) + ";";

    // Execute SQL statement
    rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);

    //if there was an error in the table deletion throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot delete from table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw DatabaseException(tmp.str(), databaseError::remove);
    }
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
 * @throw DatabaseException in case of database errors (cannot update)
 *
 * @author Michele Crepaldi s269551
 */
void server::Database::update(std::string &username, std::string &mac, const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime, const std::string &hash) {
    int rc;
    char *zErrMsg = nullptr;
    //Create SQL statement
    std::string sql =   "UPDATE savedFiles SET size="
                        + quotesql(std::to_string(size)) + ", type="
                        + quotesql(type) + ", lastWriteTime="
                        + quotesql(lastWriteTime) + ", hash="
                        + quotesql(hash) + " WHERE path="
                        + quotesql(path) + " AND username="
                        + quotesql(username) + " AND mac="
                        + quotesql(mac) + ";";

    // Execute SQL statement
    rc = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &zErrMsg);

    //if there was an error in the table insertion throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot update value in table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw DatabaseException(tmp.str(), databaseError::update);
    }
}

/**
 * function used to update an element of the database for a specified user-mac
 *
 * @param username username
 * @param mac mac of the client
 * @param d element to be update
 *
 * @throw DatabaseException in case of database errors (cannot update)
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