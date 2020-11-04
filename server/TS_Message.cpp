//
// Created by michele on 04/11/2020.
//

#include <sstream>
#include "TS_Message.h"

std::mutex TS_Message::access;

void TS_Message::print(std::ostream &out, const std::string &head, const std::string &body, const std::string &tail) {
    std::lock_guard l(access);
    std::stringstream message;
    message << "[" << head << "] -\t" << body << " -\t" << tail << std::endl;
    out << message.str();
}
