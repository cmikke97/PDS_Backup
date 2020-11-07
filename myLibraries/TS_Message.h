//
// Created by michele on 04/11/2020.
//

#ifndef SERVER_TS_MESSAGE_H
#define SERVER_TS_MESSAGE_H


#include <iostream>
#include <mutex>

/**
 * simple class to print to ostream in a thread safe way
 *
 * @author Michele Crepaldi s269551
 */
class TS_Message {
    static std::mutex access;   //to guarrantee thread safety
    std::string head, body, tail, time;
    int perc;

public:
    static void print(std::ostream &out, const std::string &head, const std::string &body, const std::string &tail);
    TS_Message(std::string head, std::string body, std::string tail);
    void update(int newPerc);
    friend std::ostream & operator << (std::ostream &out, const TS_Message &c);
};


#endif //SERVER_TS_MESSAGE_H
