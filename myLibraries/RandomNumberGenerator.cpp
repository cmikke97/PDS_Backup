//
// Created by michele on 01/11/2020.
//

#include "RandomNumberGenerator.h"

RandomNumberGenerator::RandomNumberGenerator() {
    int ret = wc_InitRng(&rng);
    if(ret != 0)
        throw RNGException("Cannot initialize RNG", RNGError::init);
}

RandomNumberGenerator::~RandomNumberGenerator() {
    wc_FreeRng(&rng);
}

void RandomNumberGenerator::getRandom(char *block, int sz) {
    int ret = wc_RNG_GenerateBlock(&rng, reinterpret_cast<byte *>(block), sz);
    if(ret != 0)
        throw RNGException("Cannot get block from RNG", RNGError::generate);
}

std::string RandomNumberGenerator::getRandomString(int size) {
    char tmp[size];
    getRandom(tmp, size);
    return std::string(tmp, size);
}
