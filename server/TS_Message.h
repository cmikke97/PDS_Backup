//
// Created by michele on 04/11/2020.
//

#ifndef SERVER_TS_MESSAGE_H
#define SERVER_TS_MESSAGE_H


#include <iostream>
#include <mutex>

class TS_Message {
    static std::mutex access;

public:
    static void print(std::ostream &out, const std::string &head, const std::string &body, const std::string &tail);
};


#endif //SERVER_TS_MESSAGE_H
