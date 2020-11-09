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
 * <p>The format used is the following (if the TAIL is "" it is not printed):</p>
 * <p>(TIME) - [HEAD] - BODY - TAIL</p>
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
    message << "(" << std::put_time(gmt, "%F %T") << ") - [" << head << "] - " << body;

    if(!tail.empty())   //the tail is optional
        message << " - " << tail;

    message << std::endl;
    out << message.str();
}

/**
 * static function to print to ostream in a thread safe way.
 * <p>The format used is the following (the TAIL is optional):</p>
 * <p>(TIME) - [HEAD] - BODY</p>
 *
 * @param out ostream to print to
 * @param head head of the message
 * @param body body of the message
 * @param tail tail of the message
 *
 * @author Michele Crepaldi s269551
 */
void TS_Message::print(std::ostream &out, const std::string &head, const std::string &body) {
    std::lock_guard l(access);
    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm *gmt = std::localtime(&time);
    std::stringstream message;
    message << "(" << std::put_time(gmt, "%F %T") << ") - [" << head << "] - " << body << std::endl;
    out << message.str();
}

/**
 * TS Message constructor, this way the message will contain a progress percentage, if you don't want it use the static
 * method TS_Message::print. The TS_Message object is to be used with << operator.
 * <p>The format used is the following:</p>
 * <p>(TIME) - [HEAD] - BODY [          ] xxx% - TAIL
 *
 * @param head head of the message
 * @param body body of the message
 * @param tail tail of the message
 *
 * @author Michele Crepaldi s269551
 */
TS_Message::TS_Message(std::string head, std::string body, std::string tail) :
        head(std::move(head)), body(std::move(body)), tail(std::move(tail)), perc(0) {

    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm *gmt = std::localtime(&t);
    std::stringstream message;
    message << std::put_time(gmt, "%F %T");
    time = message.str();
}

/**
 * update the progress percentage
 *
 * @param newPerc new percentage
 *
 * @author Michele Crepaldi s269551
 */
void TS_Message::update(int newPerc){
    perc = newPerc;
}

/**
 * TS_message operator<< overload; used to print the message (always in a thread safe way)
 *
 * @param out ostream to print to
 * @param m TS_Message to print
 * @return ostream
 *
 * @author Michele Crepaldi s269551
 */
std::ostream& operator<< (std::ostream &out, const TS_Message &m){
    std::lock_guard l(TS_Message::access);
    std::stringstream message;
    std::string progressBar(10, ' ');

    for(int i=0; 10*(i+1) <= m.perc; i+=1)
        progressBar[i] = '=';

    message << "\r(" << m.time << ") - [" << m.head << "] - " << m.body << " [" << progressBar << "] " << std::setw(3) << m.perc << "% - " << m.tail;
    out << message.str() << std::flush;

    return out;
}