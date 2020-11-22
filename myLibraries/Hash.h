//
// Created by Michele Crepaldi s269551 on 14/10/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#ifndef HASH_H
#define HASH_H

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <mutex>


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Hash class
 */

/**
 * Hash class. Class used to get and compare hashes
 *
 *  <p> This class is immutable, meaning that no methods to modify the hash are provided. The only way to create a hash
 *  object are<ul>
 *  <li>through this class Hash() constructor passing a string (or a char buffer and its size) which contains
 *  a pre-calculated hash
 *  <li>using HashMaker class to calculate the hash from strings of bytes and then returning the created hash
 *
 * @author Michele Crepaldi s269551
 */
class Hash {
public:
    Hash(const Hash &) = default;               //default copy constructor
    Hash& operator=(const Hash &) = default;    //default copy assignment
    Hash(Hash &&) = default;                    //default move constructor
    Hash& operator=(Hash &&) = default;         //default move assignment
    ~Hash() = default;                          //default destructor

    Hash() = default;                       //default empty constructor
    explicit Hash(const std::string& h);    //constructor with a string
    Hash(const char *buf, size_t len);      //constructor with a char buffer and its length

    bool operator==(Hash& h);       //operator== overload

    std::pair<char*, size_t> get(); //hash getter (as char buffer + its size)
    std::string str();              //hash geter (as string)

private:
    char _shaSum[SHA256_DIGEST_SIZE]{};  //hash (sha summary)
};

/**
 * implementation of the not equal operator for the Hash class
 * @param h1 first hash to compare
 * @param h2 second hash to compare
 * @return true if they are different, false otherwise
 *
 * @author Michele Crepaldi s269551
 */
static bool operator!=(Hash &h1, Hash& h2){
    return !(h1==h2);
}


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * HashMaker class
 */

/**
 * HashMaker class. Class used to create and return an (immutable) Hash object
 *
 * @author Michele Crepaldi s269551
 */
class HashMaker {
public:
    HashMaker();    //empty constructor
    explicit HashMaker(const std::string& data);    //constructor with an initial string
    HashMaker(const char *buf, size_t len);         //constructor with an initial char buffer

    void update(const char *buf, size_t len);   //method to update the hash with a char buffer
    void update(const std::string &buf);        //method to update the hash with a string
    Hash get();     //method to get the resulting Hash object

private:
    char _shaSum[SHA256_DIGEST_SIZE]{};  //hash result (sha summary)
    Sha256 _sha{};   //wolfSSL Sha256 object, to be used to calculate the hash

    void _init();    //method to init the wolfSSL Sha256 object
    void _finalize();    //method to finalize the wolfSSL Sha256 object
};


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * HashException class
 */

/**
 * HashError class: it describes (enumerically) all the possible hash errors
 *
 * @author Michele Crepaldi s269551
 */
enum class HashError{
    //could not create a hash object from the given buffer (it has wrong length)
    set,
    //the wolfSSL Sha256 object could not be initialized
    init,
    //the wolfSSL Sha256 object could not be update with the provided data
    update,
    //the wolfSSL Sha256 object could not be finalized
    finalize
};

/**
 * HashException class that may be returned by the Hash and/or HashMaker classes
 *  (derives from runtime_error)
 *
 * @author Michele Crepaldi s269551
 */
class HashException : public std::runtime_error {
public:

    /**
     * hash exception constructor
     *
     * @param msg the error message
     *
     * @author Michele Crepaldi s269551
     */
    HashException(const std::string& msg, HashError code):
            std::runtime_error(msg), _code(code){
    }

    /**
     * hash exception destructor
     *
     * @author Michele Crepaldi s269551
     */
    ~HashException() noexcept override = default;

    /**
     * function to retrieve the error code from the exception
     *
     * @return hash error code
     *
     * @author Michele Crepaldi s269551
     */
    HashError getCode() const noexcept{
        return _code;
    }

private:
    HashError _code; //code describing the error
};

#endif //HASH_H
