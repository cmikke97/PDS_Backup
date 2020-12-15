//
// Created by Michele Crepaldi s269551 on 01/11/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#include "RandomNumberGenerator.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * RandomNumberGenerator class methods
 */

/**
 * RandomNumberGenerator constructor
 *
 * @throws RngException:
 *  <b>init</b> in case the RNG could not be initialized
 *
 * @author Michele Crepaldi s269551
 */
RandomNumberGenerator::RandomNumberGenerator() {
    //initialize the wolfSSL random number generator (RNG)

    int ret = wc_InitRng(&rng); //wolfSSL method return code
    if(ret != 0)
        throw RngException("Cannot initialize RNG", RngError::init);
}

/**
 * RandomNumberGenerator destructor
 *
 * @author Michele Crepaldi s269551
 */
RandomNumberGenerator::~RandomNumberGenerator() {
    wc_FreeRng(&rng);   //free the wolfSSL random number generator (RNG)
}

/**
 * method used to get a random generated number as a char array
 *
 * @param block char array where to put the randomly generated number
 * @param sz size (bytes) of the randomly generated number
 *
 * @throws RngException:
 *  <b>generate</b> in case the RNG could not generate a random number
 *
 * @author Michele Crepaldi s269551
 */
void RandomNumberGenerator::getRandom(char *block, unsigned int size) {
    //generate a block of size "size" of randomly generated bytes

    int ret = wc_RNG_GenerateBlock(&rng, reinterpret_cast<byte *>(block), static_cast<int>(size)); //wolfSSL method return code
    if(ret != 0)
        throw RngException("Cannot get block from RNG", RngError::generate);
}

/**
 * method used to get a random generated number as a byte string
 *
 * @param size number of bytes to randomly generate
 * @return byte string representation of the generated number
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::getRandomString(unsigned int size) {
    //generate a string of size "size" of randomly generated bytes

    char tmp[size];
    getRandom(tmp, size);
    return std::string(tmp, size);
}

/**
 * method used to get the hex representation of a random generated number
 *
 * @param size number of bytes to randomly generate (the hex representation will be 2*size bytes long)
 * @return hex representation of the generated random number
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::getHexString(unsigned int size){
    return string_to_hex(getRandomString(static_cast<int>(size)));
}

/**
 * staic function used to convert a string of random bytes to the corresponding hexadecimal string representation
 *
 * @param input byte string
 * @return hexadecimal representation of the string
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::string_to_hex(const std::string& input)
{
    static const char hex_digits[] = "0123456789ABCDEF";    //hex digits for conversion

    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input)   //for each char in the original string
    {
        output.push_back(hex_digits[c >> 4]);   //get the upper hex digit
        output.push_back(hex_digits[c & 15]);   //get the lower hex digit
    }
    return std::move(output);
}

/**
 * lookup table from ASCII char to its byte value
 *  <p><b>examples:</p>
 *  <p>('0' = 48, hexvalues[48] = 0)
 *  <p>('A' = 65, hexvalues[65] = 10)
 *
 * @param hex_digit hex digit to convert to byte
 * @return byte representation of the hex digit passed as input
 *
 * @throws invalid_argument if the input char is not one of the allowed ones
 *
 * @author Michele Crepaldi s269551
 */
int RandomNumberGenerator::hex_value(unsigned char hex_digit)
{
    //lookup table from ASCII char to its byte value (for example '0' = 48, hexvalues[48] = 0)
    //(another example 'A' = 65, hexvalues[65] = 10)
    static const signed char hex_values[256] = {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
            -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    int value = hex_values[hex_digit];      //get the corresponding value from lookup table
    if (value == -1)    //if the input char is not one of the allowed ones
        throw std::invalid_argument("invalid hex digit");
    return value;
}

/**
 * static function to get the original byte string from the hexadecimal representation
 *
 * @param input byte string in hexadecimal representation
 * @return byte string in bytes
 *
 * @throws invalid_argument if the string length is odd
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::hex_to_string(const std::string& input)
{
    const auto len = input.length();
    if (len%2 == 1) //if the length of the string is odd
        throw std::invalid_argument("odd length");

    std::string output;
    output.reserve(len / 2);
    for (auto it = input.begin(); it != input.end(); ){
        int hi = hex_value(*it++);  //get byte representation from hex
        int lo = hex_value(*it++);  //get byte representation from hex
        output.push_back(hi << 4 | lo); //recompose original byte
    }
    return std::move(output);
}
