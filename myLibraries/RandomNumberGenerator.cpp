//
// Created by michele on 01/11/2020.
//

#include "RandomNumberGenerator.h"

/**
 * RandomNumberGenerator constructor
 */
RandomNumberGenerator::RandomNumberGenerator() {
    int ret = wc_InitRng(&rng);
    if(ret != 0)
        throw RNGException("Cannot initialize RNG", RNGError::init);
}

/**
 * RandomNumberGenerator destructor
 */
RandomNumberGenerator::~RandomNumberGenerator() {
    wc_FreeRng(&rng);
}

/**
 * function to get a random generated number as a char array
 *
 * @param block char array where to put the randomly generated number
 * @param sz size (bytes) of the randomly generated number
 *
 * @author Michele Crepaldi s269551
 */
void RandomNumberGenerator::getRandom(char *block, int sz) {
    int ret = wc_RNG_GenerateBlock(&rng, reinterpret_cast<byte *>(block), sz);
    if(ret != 0)
        throw RNGException("Cannot get block from RNG", RNGError::generate);
}

/**
 * function to get a random generated number as a byte string
 *
 * @param size size (bytes) of the randomly generated number
 *
 * @return byte string representation of the generated number
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::getRandomString(int size) {
    char tmp[size];
    getRandom(tmp, size);
    return std::string(tmp, size);
}

/**
 * function used to get the hex representation of a random generated number
 * @param size size (bytes) of the randomly generated number (the hex representation will be 2*size bytes long)
 *
 * @return hex representation of the generated number
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::getHexString(int size){
    return string_to_hex(getRandomString(size));
}

/**
 * function to convert a string of random bytes to the corresponding hexadecimal string representation
 *
 * @param input byte string
 * @return hexadecimal representation of the string
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::string_to_hex(const std::string& input)
{
    static const char hex_digits[] = "0123456789ABCDEF";    //define the hex digits for convertion

    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input)   //for each char in the original string
    {
        output.push_back(hex_digits[c >> 4]);   //to get the upper hex digit
        output.push_back(hex_digits[c & 15]);   //to get the lower hex digit
    }
    return output;
}

/**
 * lookup table from ASCII char to its byte value
 * <p><b>examples:</p>
 * <p>('0' = 48, hexvalues[48] = 0)
 * <p>('A' = 65, hexvalues[65] = 10)
 *
 * @param hex_digit hex digit to convert to byte
 * @return byte representation of the hex digit passed as input
 *
 * @throw invalid_argument if the input char is not one of the allowed ones
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
    if (value == -1) throw std::invalid_argument("invalid hex digit");  //if the input char is not one of the allowed ones
    return value;
}

/**
 * function to get the original byte string from the hexadecimal representation
 *
 * @param input byte string in hexadecimal representation
 * @return byte string in bytes
 *
 * @throw invalid_argument if the string length is odd
 *
 * @author Michele Crepaldi s269551
 */
std::string RandomNumberGenerator::hex_to_string(const std::string& input)
{
    const auto len = input.length();
    if (len & 1) throw std::invalid_argument("odd length");

    std::string output;
    output.reserve(len / 2);
    for (auto it = input.begin(); it != input.end(); )
    {
        int hi = hex_value(*it++);
        int lo = hex_value(*it++);
        output.push_back(hi << 4 | lo);
    }
    return output;
}
