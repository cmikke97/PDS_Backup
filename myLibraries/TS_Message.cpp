//
// Created by michele on 04/11/2020.
//

#include <sstream>
#include <iomanip>
#include <utility>
#include "TS_Message.h"

std::mutex TS_Message::access;

/**
 * static function to print to ostream in a thread safe way.
 * <p>the format used is the following:</p>
 * <p>(TIME) - [HEAD] - BODY - TAIL
 *
 * @param out ostream to print to
 * @param head head of the message
 * @param body body of the message
 * @param tail tail of the message
 *
 * @author Michele Crepaldi s269551
 */
void TS_Message::print(std::ostream &out, const std::string &head, const std::string &body, const std::string &tail) {
    std::lock_guard l(access);
    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm *gmt = std::localtime(&time);
    std::stringstream message;
    message << "(" << std::put_time(gmt, "%F %T") << ") - [" << head << "] - " << body << " - " << tail << std::endl;
    out << message.str();
}

TS_Message::TS_Message(std::string head, std::string body, std::string tail) :
        head(std::move(head)), body(std::move(body)), tail(std::move(tail)), perc(0) {

    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm *gmt = std::localtime(&t);
    std::stringstream message;
    message << std::put_time(gmt, "%F %T");
    time = message.str();
}

void TS_Message::update(int newPerc){
    perc = newPerc;
}

std::ostream& operator << (std::ostream &out, const TS_Message &m){
    std::lock_guard l(TS_Message::access);
    std::stringstream message;
    std::string progressBar(10, ' ');

    for(int i=0; 10*(i+1) <= m.perc; i+=1)
        progressBar[i] = '=';

    message << "\r(" << m.time << ") - [" << m.head << "] - " << m.body << " [" << progressBar << "] " << std::setw(3) << m.perc << "% - " << m.tail;
    out << message.str() << std::flush;

    return out;
}