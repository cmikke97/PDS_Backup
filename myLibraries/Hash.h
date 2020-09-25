//
// Created by michele on 27/07/2020.
//

#ifndef CLIENT_HASH_H
#define CLIENT_HASH_H

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <mutex>

/**
 * Hash object -> used to compute, get and compare an hash of some data
 */
class Hash {
    unsigned char md_value[EVP_MAX_MD_SIZE]{};
    unsigned int md_len{};
    EVP_MD_CTX *mdctx;

public:
    Hash();
    Hash(const unsigned char *buf, unsigned long len);
    void HashInit();
    void HashUpdate(char *buf, unsigned long len);
    void HashFinalize();
    std::pair<unsigned char*, unsigned long> getValue();
    bool operator==(Hash& h);
};


#endif //CLIENT_HASH_H
