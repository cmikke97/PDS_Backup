//
// Created by michele on 14/10/2020.
//

#ifndef CLIENT_HASH_H
#define CLIENT_HASH_H

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <mutex>
/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Hash class
 */

/**
 * Hash class -> used to get, compare hashes
 *
 * @author Michele Crepaldi s269551
 */
class Hash {
    char shaSum[SHA256_DIGEST_SIZE]{};

public:
    Hash() = default;
    explicit Hash(const std::string& h);
    Hash(const char *buf, size_t len);
    bool operator==(Hash& h);
    std::pair<char*, size_t> get();
    std::string str();
};

/**
 * implmentation of the not equal operator for the Hash class
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
 * HashMaker class -> used to create an Hash object
 *
 * @author Michele Crepaldi s269551
 */
class HashMaker {
    char shaSum[SHA256_DIGEST_SIZE]{};
    Sha256 sha{};

    void init();
    void finalize();

public:
    HashMaker();
    explicit HashMaker(const std::string& data);
    HashMaker(const char *buf, size_t len);

    void update(const char *buf, size_t len);
    void update(std::string buf);
    Hash get();
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * exceptions
 */

/**
 * hashError class: it describes (enumerically) all the possible hash errors
 *
 * @author Michele Crepaldi s269551
 */
enum class hashError{set, init, update, finalize};

/**
 * exceptions for the hash class
 *
 * @author Michele Crepaldi s269551
 */
class HashException : public std::runtime_error {
    hashError code;
public:

    /**
     * hash exception constructor
     *
     * @param msg the error message
     *
     * @author Michele Crepaldi s269551
     */
    HashException(const std::string& msg, hashError code):
            std::runtime_error(msg), code(code){
    }

    /**
     * hash exception destructor.
     *
     * @author Michele Crepaldi s269551
     */
    ~HashException() noexcept override = default;

    /**
     * function to retrieve the error code from the exception
     *
     * @return error code
     *
     * @author Michele Crepaldi s269551
     */
    hashError getCode() const noexcept{
        return code;
    }
};

#endif //CLIENT_HASH_H
