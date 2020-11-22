//
// Created by Michele Crepaldi s269551 on 01/11/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#ifndef RANDOMNUMBERGENERATOR_H
#define RANDOMNUMBERGENERATOR_H

#include <string>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/random.h>
#include <stdexcept>


/**
 * RandomNumberGenerator class. It is used to get a cryptographically secure random number/bitstring
 *
 * @author Michele Crepaldi s269551
 */
class RandomNumberGenerator {
public:
    RandomNumberGenerator(const RandomNumberGenerator &) = delete;    //copy constructor deleted
    RandomNumberGenerator& operator=(const RandomNumberGenerator &) = delete; //assignment deleted
    RandomNumberGenerator(RandomNumberGenerator &&) = delete; //move constructor deleted
    RandomNumberGenerator& operator=(RandomNumberGenerator &&) = delete;  //move assignment deleted

    RandomNumberGenerator();    //constructor
    ~RandomNumberGenerator();   //destructor

    //methods

    void getRandom(char *buf, unsigned int size);
    std::string getRandomString(unsigned int size);
    std::string getHexString(unsigned int size);
    static std::string string_to_hex(const std::string& input);
    static std::string hex_to_string(const std::string& input);

private:
    RNG rng{};  //wolfSSL random number generator instance

    static int hex_value(unsigned char hex_digit);
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * exceptions
 */

/**
 * RngError class: it describes (enumerically) all the possible RandomNumberGenerator errors
 *
 * @author Michele Crepaldi s269551
 */
enum class RngError{
    //error during rng initialization
    init,

    //error during random number generation
    generate
};

/**
 * RngException exception class that may be returned by the RandomNumberGenerator class
 *
 * @author Michele Crepaldi s269551
 */
class RngException : public std::runtime_error {
public:

    /**
     * rng exception constructor
     *
     * @param msg the error message
     * @param code RNGError code
     *
     * @author Michele Crepaldi s269551
     */
    RngException(const std::string& msg, RngError code):
            std::runtime_error(msg), _code(code){
    }

    /**
     * rng exception destructor
     *
     * @author Michele Crepaldi s269551
     */
    ~RngException() noexcept override = default;

    /**
     * function to retrieve the error code from the exception
     *
     * @return rng error code
     *
     * @author Michele Crepaldi s269551
     */
    RngError getCode() const noexcept{
        return _code;
    }

private:
    RngError _code;
};


#endif //RANDOMNUMBERGENERATOR_H
