//
// Created by michele on 28/09/2020.
//

#include "Database.h"

/**
 * destructor of class Database
 *
 * @author Michele Crepaldi s269551
 */
Database::~Database() {
    sqlite3_close(db);
}

/**
 * function used to open the connection to a sqlite3 database; if the database already existed then it opens it, otherwise it also creates the needed table
 *
 * @param path path of the database in the filesystem
 *
 * @throw runtime_error in case of database errors (cannot open)
 *
 * @author Michele Crepaldi s269551
 */
void Database::open(const std::string &path) {
    int rc;
    //check if the db already exists before opening it (open will create a new db if none is found)
    bool create = !std::filesystem::directory_entry(path).exists();

    //open the database
    rc = sqlite3_open("../clientDB/clientDB.sqlite", &db);

    //in case of errors throw an exception
    if( rc ) {
        std::stringstream tmp;
        tmp << "Cannot open database: " << sqlite3_errmsg(db);
        throw std::runtime_error(tmp.str());
    }

    //if the db is new then create the table inside it
    if(create){
        char *zErrMsg = nullptr;
        //Create SQL statement
        std::string sql = "CREATE TABLE \"savedFiles\" (/"
                          "\"id\" INTEGER,/"
                          "\"path\" TEXT UNIQUE,/"
                          "\"size\" INTEGER,/"
                          "\"type\" TEXT,/"
                          "\"lastWriteTime\" TEXT,/"
                          "PRIMARY KEY(\"id\" AUTOINCREMENT)/"
                          ");";

        //Execute SQL statement
        rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrMsg);

        //if there was an error in the table creation throw an exception
        if( rc != SQLITE_OK ){
            std::stringstream tmp;
            tmp << "Cannot create table: " << zErrMsg;
            sqlite3_free(zErrMsg);
            throw std::runtime_error(tmp.str());
        }
    }
}

/**
 * function used to apply a function to each row of the database
 *
 * @param f function to be used for each row extracted from the database
 *
 * @throw runtime_error in case of database errors (cannot prepare or read)
 *
 * @author Michele Crepaldi s269551
 */
void Database::forAll(std::function<void (const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime)> &f) {
    int rc;
    char *zErrMsg = nullptr;
    //statement handle
    sqlite3_stmt* stmt;
    //Create SQL statement
    std::string sql = "SELECT path, size, type, lastWriteTime from savedFiles;";

    //Prepare SQL statement
    rc = sqlite3_prepare(db, sql.c_str(), -1, &stmt, nullptr);

    //if there was an error in the SQL statement prepare throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot prepare table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw std::runtime_error(tmp.str());
    }

    //prepare some variables
    bool done = false;
    std::string path, type, lastWriteTime;
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

                //use function provided
                f(path, type, size, lastWriteTime);
                break;

            case SQLITE_DONE:   //in case there are no more rows
                done = true;
                break;

            default:    //in any other case -> error (throw exception)
                std::stringstream tmp;
                tmp << "Cannot read table: " << zErrMsg;
                sqlite3_free(zErrMsg);
                throw std::runtime_error(tmp.str());
        }
    }

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
 * @throw runtime_error in case of database errors (cannot insert)
 *
 * @author Michele Crepaldi s269551
 */
void Database::insert(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime) {
    int rc;
    char *zErrMsg = nullptr;
    //Create SQL statement
    std::string sql =   "INSERT INTO savedFiles (path, size, type, lastWriteTime) VALUES ("
            + quotesql(path) + ","
            + quotesql(std::to_string(size)) + ","
            + quotesql(type) + ","
            + quotesql(lastWriteTime) + ","
            + ");";

    // Execute SQL statement
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrMsg);

    //if there was an error in the table insertion throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot insert into table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw std::runtime_error(tmp.str());
    }
}

/**
 * function used to remove an element from the database
 *
 * @param path path of the element to be removed
 *
 * @throw runtime_error in case of database errors (cannot delete)
 *
 * @author Michele Crepaldi s269551
 */
void Database::remove(const std::string &path) {
    int rc;
    char *zErrMsg = nullptr;
    //Create SQL statement
    std::string sql =   "DELETE FROM savedFiles WHERE path="
                        + quotesql(path) + ";";

    // Execute SQL statement
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrMsg);

    //if there was an error in the table deletion throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot delete from table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw std::runtime_error(tmp.str());
    }
}

/**
 * function used to update an element of the database
 *
 * @param path path of the element to be update
 * @param type type of the element to be update
 * @param size size of the element to be update
 * @param lastWriteTime last write time of the element to be update
 *
 * @throw runtime_error in case of database errors (cannot update)
 *
 * @author Michele Crepaldi s269551
 */
void Database::update(const std::string &path, const std::string &type, uintmax_t size, const std::string &lastWriteTime) {
    int rc;
    char *zErrMsg = nullptr;
    //Create SQL statement
    std::string sql =   "UPDATE savedFiles SET size="
                        + quotesql(std::to_string(size)) + ", type="
                        + quotesql(type) + ", lastWriteTime="
                        + quotesql(lastWriteTime) + " WHERE path="
                        + quotesql(path) + ";";

    // Execute SQL statement
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrMsg);

    //if there was an error in the table insertion throw an exception
    if( rc != SQLITE_OK ){
        std::stringstream tmp;
        tmp << "Cannot update value in table: " << zErrMsg;
        sqlite3_free(zErrMsg);
        throw std::runtime_error(tmp.str());
    }
}
