//
// Created by michele on 28/07/2020.
//

#ifndef CLIENT_MESSAGE_H
#define CLIENT_MESSAGE_H


#include <string>
#include <cstring>

/**
 * Message object
 *
 * @author Michele Crepaldi s269551
 */
class Message {
    char* message{};
    unsigned long size{};
    unsigned long pos{};

public:
    Message();
    explicit Message(unsigned long size);
    Message(const Message& other);
    Message(Message&& other) noexcept;
    Message& operator=(Message other);
    ~Message();
    void append(const char* str, unsigned long len);
    //template<> void append<const std::string&>(const std::string& element);
    template <typename T> void append(T element);
    char* getMessage();
    unsigned long getSize();
    unsigned long getCapacity();
    friend void swap(Message& a, Message& b);
};

/**
 * append element of generic type T to message
 *
 * @tparam T type of the element to append
 * @param element to append
 * @return false if the string could not be appended (it was too long), true otherwise
 *
 * @author Michele Crepaldi s269551
 */
template<typename T>
void Message::append(T element) {
    char element_c[sizeof(T)];
    memcpy(element_c,&element,sizeof(T));
    append(element_c, sizeof(T));
}


#endif //CLIENT_MESSAGE_H
