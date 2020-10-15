//
// Created by michele on 14/10/2020.
//

#include <wolfssl/wolfcrypt/error-crypt.h>
#include <sstream>
#include "Hash.h"

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Hash class
 */

/**
 * hash constructor from a buffer and its length
 *
 * @param buf buffer where to find data
 * @param len length of the data
 *
 * @return result of memcmp
 */
Hash::Hash(const char *buf, size_t len) {
    memcpy(shaSum, buf, len);
}

/**
 * implementation of a constant time memcmp (to avoid side channel timing attacks)
 * (taken from openSSL since wolfSSL does not support it yet)
 *
 * @param b1 first hash
 * @param b2 second hash
 * @param len length of the two hashes (must be equal)
 * @return result of memcmp
 */
int CRYPTO_memcmp(const void * in_a, const void * in_b, size_t len)
{
    size_t i;
    const volatile auto *a = static_cast<volatile const unsigned char *>(in_a);
    const volatile auto *b = static_cast<volatile const unsigned char *>(in_b);
    unsigned char x = 0;

    for (i = 0; i < len; i++)
        x |= a[i] ^ b[i];

    return x;
}

/**
 * function to compare the two hashes in constant time
 *
 * @param other hash to compare with this one
 * @return whether the two hashes are the same or not
 *
 * @author Michele Crepaldi s269551
 */
bool Hash::operator==(Hash &other) {
    auto myHash = this->str();  //get my hash value
    auto otherHash = other.str();   //get the other's hash value
    if(myHash.size() == otherHash.size()) //if the size is the same (they should be unless there are errors)
        return CRYPTO_memcmp(myHash.c_str(), otherHash.c_str(), myHash.length()); //then compare the two hashes in constant time
    return false;
}

/**
 * get the hash associated to this Hash object as a pair of char* and len
 *
 * @return the hash and len
 *
 * @author Michele Crepaldi s269551
 */
std::pair<char*, size_t> Hash::get() {
    return std::make_pair(shaSum, sizeof(shaSum));
}

/**
 * get the hash associated to this Hash object as a string
 *
 * @return the hash as string
 *
 * @author Michele Crepaldi s269551
 */
std::string Hash::str() {
    return std::string(shaSum,sizeof(shaSum));
}

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * HashMaker class
 */

/**
 * function to handle Crypto errors (Openssl)
 *
 * @author Michele Crepaldi s269551
 */
void handleErrors(int err, const std::string& msg, hashError type)
{
    std::stringstream errMsg;
    errMsg << msg << ": " << wc_GetErrorString(err);
    throw HashException(errMsg.str(), type);
}

/**
 * default constructor of HashMaker class
 *
 * @throw HashException in case it cannot initialize the HashMaker object
 *
 * @author Michele Crepaldi s269551
 */
HashMaker::HashMaker() {
    init();
};

/**
 * constructor of this HashMaker object with a string as input, it can be used to initialize the hash with already a value;
 * it is still possible to update.
 *
 * @param string to be hashed
 *
 * @throw HashException in case it cannot initialize the hash object
 * @throw HashException in case it cannot update the hash object
 *
 * @author Michele Crepaldi s269551
 */
HashMaker::HashMaker(const std::string& buffer) : HashMaker(buffer.data(), buffer.size()) {
}

/**
 * constructor of this HashMaker object with a char * (and its length) as input, it can be used to initialize the hash
 * with already a value; it is still possible to update.
 *
 * @param buf data to be hashed
 * @param len of the data
 *
 * @throw HashException in case it cannot initialize the hash object
 * @throw HashException in case it cannot update the hash object
 *
 * @author Michele Crepaldi s269551
 */
HashMaker::HashMaker(const char *buf, size_t len) : HashMaker() {
    update(buf, len);
}

/**
 * function to initialize the HashMaker object
 *
 * @throw HashException in case it cannot initialize the HashMaker object
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::init() {
    int err = wc_InitSha256(&sha);
    if(err != 0)
        handleErrors(err, "Cannot initialize hash", hashError::init);
}

/**
 * function to update the current HashMaker with a block of data
 *
 * @param buf buffer containing data to be hashed
 * @param len length of the buffer
 *
 * @throw HashException in case it cannot update the HashMaker object
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::update(const char *buf, size_t len) {
    int err = wc_Sha256Update(&sha, reinterpret_cast<const byte *>(buf), len);
    if(err != 0)
        handleErrors(err, "Cannot update hash", hashError::update);
}

/**
 * function to update the current HashMaker with a block of data
 *
 * @param buf string buffer containing data to be hashed
 *
 * @throw HashException in case it cannot update the HashMaker object
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::update(std::string buf) {
    update(buf.data(), buf.size());
}

/**
 * function to finalize the content of the HashMaker after hashing the last block
 * <p>
 * <b>(it is necessary to use this!)
 *
 * @throw HashException in case it cannot finalize the HashMaker object
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::finalize() {
    int err = wc_Sha256Final(&sha, reinterpret_cast<byte *>(shaSum));
    if(err != 0)
        handleErrors(err, "Cannot finalize hash", hashError::finalize);
}

/**
 * function to get the final Hash object constructed by the HashMaker
 *
 * @return Hash constructed
 *
 * @throw HashException in case it cannot finalize the HashMaker object
 *
 * @author Michele Crepaldi s269551
 */
Hash HashMaker::get() {
    finalize();
    return Hash(shaSum, sizeof(shaSum));
}
