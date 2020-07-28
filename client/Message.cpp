//
// Created by michele on 28/07/2020.
//

#include "Message.h"

/**
 * constructor of the Message object
 *
 * @param size of the message
 *
 * @author Michele Crepaldi s269551
 */
Message::Message(unsigned long size) : size(size), pos(0){
    message = new char[size];
}

/**
 * destructor of the Message object
 *
 * @author Michele Crepaldi s269551
 */
Message::~Message() {
    delete[] message;
}

/**
 * append c string to message
 *
 * @param str to append
 * @param len of the string to append
 * @return false if the string could not be appended (it was too long), true otherwise
 *
 * @author Michele Crepaldi s269551
 */
void Message::append(const char *str, unsigned long len) {
    if(pos+len >= size)
        return; //TODO exception or error code from this condition (that should not happen)

    for(int i=0; i < len && pos+i < size; i++){
        message[pos+i] = str[i];
    }
    pos+=len;
}

/**
 * append c++ string to message
 *
 * @param str to append
 * @return false if the string could not be appended (it was too long), true otherwise
 *
 * @author Michele Crepaldi s269551
 */
template<>
void Message::append<const std::string&>(const std::string& element) {
    if(pos+element.length() >= size)
        return; //TODO exception or error code from this condition (that should not happen)

    for(int i=0; i<element.length() && pos+i < size; i++){
        message[pos+i] = element[i];
    }
    pos+=element.length();
}

/**
 * get the message
 *
 * @return message
 *
 * @author Michele Crepaldi s269551
 */
char *Message::getMessage() {
    return message;
}

/**
 * get the size of the message
 *
 * @return size
 *
 * @author Michele Crepaldi s269551
 */
unsigned long Message::getSize() {
    return pos+1;
}

/**
 * get the capacity of the message object
 *
 * @return capacity
 *
 * @author Michele Crepaldi s269551
 */
unsigned long Message::getCapacity() {
    return size;
}

/**
 * copy constructor
 *
 * @author Michele Crepaldi s269551
 */
Message::Message(const Message &other) : size(other.size), pos(other.pos) {
    message = new char[size];
    memcpy(message, other.message, size);
}

/**
 * operator= (by copy or move)
 *
 * @param other message
 * @return message (this)
 *
 * @author Michele Crepaldi s269551
 */
Message &Message::operator=(Message other) {
    swap(*this,other);
    return *this;
}

/**
 * swap function
 *
 * @param a message
 * @param b message
 *
 * @author Michele Crepaldi s269551
 */
void swap(Message &a, Message &b) {
    std::swap(a.size, b.size);
    std::swap(a.pos, b.pos);
    std::swap(a.message, b.message);
}

/**
 * movement constructor
 *
 * @param other message
 *
 * @author Michele Crepaldi s269551
 */
Message::Message(Message &&other) noexcept : size(0), pos(0), message(nullptr) {
    swap(*this,other);
}

/**
 * default constructor
 *
 * @author Michele Crepaldi s269551
 */
Message::Message() = default;
