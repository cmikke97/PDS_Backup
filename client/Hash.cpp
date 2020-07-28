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
 * constructor of this Hash object
 *
 * @param buf data to be hashed
 * @param len of the data
 *
 * @author Michele Crepaldi s269551
 */
Hash::Hash(const unsigned char *buf, unsigned long len) {
    EVP_MD_CTX *mdctx;

    ERR_load_CRYPTO_strings();

    if((mdctx = EVP_MD_CTX_new()) == nullptr)
        handleErrors();

    if(1 != EVP_MD_CTX_init(mdctx))
        handleErrors();

    if(1!=EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr))
        handleErrors();

    if(1!=EVP_DigestUpdate(mdctx, buf, len))
        handleErrors();

    if(1!=EVP_DigestFinal_ex(mdctx, md_value, &md_len))
        handleErrors();

    EVP_MD_CTX_free(mdctx);
    EVP_cleanup();
    ERR_free_strings();
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
