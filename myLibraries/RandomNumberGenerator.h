//
// Created by michele on 01/11/2020.
//

#ifndef PROVAOPENSSL_RANDOMNUMBERGENERATOR_H
#define PROVAOPENSSL_RANDOMNUMBERGENERATOR_H


#include <string>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/random.h>
#include <stdexcept>

class RandomNumberGenerator {
    RNG rng{};

public:
    RandomNumberGenerator();
    ~RandomNumberGenerator();

    void getRandom(char *buf, int size);
    std::string getRandomString(int size);
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * exceptions
 */

/**
 * RNGError class: it describes (enumerically) all the possible random number generator errors
 *
 * @author Michele Crepaldi s269551
 */
enum class RNGError{init, generate};

/**
 * exceptions for the RNG class
 *
 * @author Michele Crepaldi s269551
 */
class RNGException : public std::runtime_error {
    RNGError code;
public:

    /**
     * RNG exception constructor
     *
     * @param msg the error message
     * @param code RNGError code
     *
     * @author Michele Crepaldi s269551
     */
    RNGException(const std::string& msg, RNGError code):
            std::runtime_error(msg), code(code){
    }

    /**
     * RNG exception destructor.
     *
     * @author Michele Crepaldi s269551
     */
    ~RNGException() noexcept override = default;

    /**
     * function to retrieve the error code from the exception
     *
     * @return error code
     *
     * @author Michele Crepaldi s269551
     */
    RNGError getCode() const noexcept{
        return code;
    }
};


#endif //PROVAOPENSSL_RANDOMNUMBERGENERATOR_H
