//
// Created by michele on 27/07/2020.
//

#include "Hash.h"

/**
 * function to handle Crypto errors (Openssl)
 *
 * @author Michele Crepaldi s269551
 */
void handleErrors()
{
    ERR_print_errors_fp(stderr);
    abort();
}

/**
 * default constructor
 *
 * @author Michele Crepaldi s269551
 */
Hash::Hash() = default;

/**
 * constructor of this Hash object in case you want an hash of a small string
 * (not suitable with files because you would need to load them entirely in memory)
 * <p>
 * for hashing a <b>file</b> use this other methods in this order:
 * <ul>
 *      <li> <b> hashInit
 *      <li> <b> hashUpdate (repeated for each block of data)
 *      <li> <b> hashFinalize
 * </ul>
 *
 * @param string to be hashed
 *
 * @author Michele Crepaldi s269551
 */
Hash::Hash(std::string data) {
    HashInit();
    HashUpdate(const_cast<char *>(data.c_str()), data.length());
    HashFinalize();
}

/**
 * constructor of this Hash object in case you want an hash of a small object
 * (not suitable with files because you would need to load them entirely in memory)
 * <p>
 * for hashing a <b>file</b> use this other methods in this order:
 * <ul>
 *      <li> <b> hashInit
 *      <li> <b> hashUpdate (repeated for each block of data)
 *      <li> <b> hashFinalize
 * </ul>
 *
 * @param buf data to be hashed
 * @param len of the data
 *
 * @author Michele Crepaldi s269551
 */
Hash::Hash(char *buf, unsigned long len) {
    HashInit();
    HashUpdate(buf, len);
    HashFinalize();
}

/**
 * get the hash associated to this Hash object
 *
 * @return the hash and len
 *
 * @author Michele Crepaldi s269551
 */
std::pair<unsigned char*, unsigned long> Hash::getValue() {
    return std::make_pair(md_value, md_len);
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
    auto myHash = this->getValue();
    auto otherHash = other.getValue();
    if(myHash.second == otherHash.second) //if the len is the same (they should be unless there are errors)
        return CRYPTO_memcmp(myHash.first, otherHash.first, myHash.second); //then compare the two hashes in constant time
    return false;
}

/**
 * function to initialize the hash object
 *
 * @author Michele Crepaldi s269551
*/
void Hash::HashInit() {
    ERR_load_CRYPTO_strings();

    if((mdctx = EVP_MD_CTX_new()) == nullptr)
        handleErrors();

    if(1 != EVP_MD_CTX_init(mdctx))
        handleErrors();

    if(1!=EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr))
        handleErrors();
}

/**
 * function to update the current hash with a block of data
 * <p>
 * <b>(don't forget to use HashFinalize after the last block!)
 *
 * @param buf buffer containing data to be hashed
 * @param len length of the buffer
 *
 * @author Michele Crepaldi s269551
*/
void Hash::HashUpdate(char *buf, unsigned long len) {
    if(1!=EVP_DigestUpdate(mdctx, buf, len))
        handleErrors();
}

/**
 * function to finalize the content of the hash after hashing the last block
 * <p>
 * <b>(it is necessary to use this!)
 *
 * @author Michele Crepaldi s269551
*/
void Hash::HashFinalize() {
    if(1!=EVP_DigestFinal_ex(mdctx, md_value, &md_len))
        handleErrors();

    EVP_MD_CTX_free(mdctx);
    EVP_cleanup();
    ERR_free_strings();
}
