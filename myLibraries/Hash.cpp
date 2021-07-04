//
// Created by Michele Crepaldi s269551 on 14/10/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#include "Hash.h"

#include <sstream>


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Hash class
 */

/**
 * Hash class constructor from a char buffer and its length
 *
 * @param buf pre-computed hash buffer to copy
 * @param len pre-computed hash buffer length
 *
 * @throws HashException:
 *  <b>set</b> in case the given buffer length is wrong (!= SHA256_DIGEST_SIZE)
 *
 * @author Michele Crepaldi s269551
 *
 */
Hash::Hash(const char *buf, size_t len) {
    if(len != SHA256_DIGEST_SIZE)
        throw HashException("Wrong buffer length, cannot construct Hash", HashError::set);

    memcpy(_shaSum, buf, len);
}

/**
 * Hash class constructor from a string buffer
 *
 * @param buf pre-computed hash string to copy
 *
 * @author Michele Crepaldi s269551
 */
Hash::Hash(const std::string& h) : Hash(h.data(), h.length()) {
}

/**
 * implementation of a constant time memcmp (to avoid side channel timing attacks)
 *  (copied from openSSL source since wolfSSL does not support it yet)
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
 * Hash class operator== overloading.
 *  Compares 2 Hash objects in constant time (to avoid side channel timing attacks)
 *
 * @param other hash to compare with this one
 * @return whether the two hashes are the same or not
 *
 * @author Michele Crepaldi s269551
 */
bool Hash::operator==(Hash &other) {
    auto myHash = this->str();      //my hash value
    auto otherHash = other.str();   //the other's hash value
    if(myHash.size() == otherHash.size())   //if the size is the same (they should be, unless there are errors)
        return !CRYPTO_memcmp(myHash.c_str(), otherHash.c_str(), myHash.length()); //then compare the two hashes in constant time
    return false;
}

/**
 * method used to get the hash associated to this Hash object as a pair of char* and len
 *
 * @return the hash (as char*) and len
 *
 * @author Michele Crepaldi s269551
 */
std::pair<char*, size_t> Hash::get() {
    return std::make_pair(_shaSum, sizeof(_shaSum));    //return the pair
}

/**
 * method used to get the hash associated to this Hash object as a string
 *
 * @return the hash as string
 *
 * @author Michele Crepaldi s269551
 */
std::string Hash::str() {
    return std::string(_shaSum, sizeof(_shaSum));
}


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * HashMaker class
 */

/**
 * utility function to handle Crypto errors (wolfSSL)
 *
 * @throws HashException:
 *  <b>[type]</b> Error provided as input
 *
 * @author Michele Crepaldi s269551
 */
void handleErrors(int err, const std::string& msg, HashError type)
{
    std::stringstream errMsg;
    errMsg << msg << ": " << wc_GetErrorString(err);    //get also the error string from wolfSSL
    throw HashException(errMsg.str(), type);            //throw exception
}

/**
 * default HashMaker constructor
 *
 * @author Michele Crepaldi s269551
 */
HashMaker::HashMaker() {
    _init();    //initialize the wolfSSL SHA256 object
}

/**
 * HashMaker constructor with a string as input, it can be used to initialize the hash with an initial value;
 *  it is still possible to update the hash.
 *
 * @param string initial string to use to compute the Hash object
 *
 * @author Michele Crepaldi s269551
 */
HashMaker::HashMaker(const std::string& buffer) : HashMaker(buffer.data(), buffer.size()) {
}

/**
 * HashMaker constructor with a char * (and its length) as input, it can be used to initialize the hash
 *  with an initial value; it is still possible to update.
 *
 * @param buf initial buffer to use to compute the Hash object
 * @param len length of the buffer
 *
 * @author Michele Crepaldi s269551
 */
HashMaker::HashMaker(const char *buf, size_t len) : HashMaker() {
    update(buf, len);
}

/**
 * method to initialize the HashMaker object
 *
 * @throws HashException:
 *  <b>init</b> in case wolfSSL Sha256 object cannot be initialized
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::_init() {
    //initialize the wolfSSL Sha256 object

    int err = wc_InitSha256(&_sha); //wolfSSL Sha256 method error code

    if(err != 0)
        handleErrors(err, "Cannot initialize hash", HashError::init);
}

/**
 * method to update the current wolfSSL Sha256 object with a block of data
 *
 * @param buf buffer containing data to be hashed
 * @param len length of the buffer
 *
 * @throws HashException:
 *  <b>update</b> in case wolfSSL Sha256 object cannot be updated with the provided data
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::update(const char *buf, size_t len) {
    //update the wolfSSL Sha256 object with the provided buffer

    int err = wc_Sha256Update(&_sha, reinterpret_cast<const byte *>(buf), len); //wolfSSL Sha256 method error code

    if(err != 0)
        handleErrors(err, "Cannot update hash", HashError::update);
}

/**
 * method to update the current wolfSSL Sha256 object with a string of data
 *
 * @param buf string buffer containing data to be hashed
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::update(const std::string &buf) {
    update(buf.data(), buf.size());
}

/**
 * method to finalize the _shaSum after hashing the last block
 *
 * @throws HashException:
 *  <b>finalize</b> in case wolfSSL Sha256 object cannot be finalized
 *
 * @author Michele Crepaldi s269551
*/
void HashMaker::_finalize() {
    //finalize the wolfSSL Sha256 object and get the final _shaSum value

    int err = wc_Sha256Final(&_sha, reinterpret_cast<byte *>(_shaSum)); //wolfSSL Sha256 method error code
    if(err != 0)
        handleErrors(err, "Cannot finalize hash", HashError::finalize);
}

/**
 * method to get the final Hash object constructed by the HashMaker
 *
 *  <p>After this method this HashMaker object should not be any more used</p>
 *
 * @return constructed Hash object
 *
 * @author Michele Crepaldi s269551
 */
Hash HashMaker::get() {
    _finalize();    //finalize the wolfSSL Sha256 object and get the _shaSum
    return Hash(_shaSum, sizeof(_shaSum));  //return the Hash object
}
