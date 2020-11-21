//
// Created by Michele Crepaldi s269551 on 04/11/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#include <sstream>
#include <iomanip>
#include "Message.h"


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Message class methods
 */

//static variables definition

std::mutex Message::_access;

/**
 * Message constructor, the message will contain a progress percentage, if you don't want it use the static
 * method TS_Message::print. The TS_Message object can be used with << operator.
 * <p>The format used is the following:</p>
 * <p>(TIME) - [HEAD] - BODY [          ] xxx% - TAIL
 *
 * @param head head of the message
 * @param body body of the message
 * @param tail tail of the message
 *
 * @author Michele Crepaldi s269551
 */
Message::Message(std::string  head, std::string  body, std::string  tail) :
        _head(std::move(head)), _body(std::move(body)), _tail(std::move(tail)), _perc(0) {

    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());    //current time
    std::tm *gmt = std::localtime(&t);  //current time as std::tm type
    std::stringstream message;
    message << std::put_time(gmt, "%F %T");
    _time = message.str();  //set _time
}

/**
 * method used to print the message (in a thread safe way)
 * <p>The format used is the following:</p>
 * <p>(TIME) - [HEAD] - BODY [          ] xxx% - TAIL
 *
 * @param out ostream to print to
 *
 * @author Michele Crepaldi s269551
 */
void Message::print(std::ostream &out){
    std::lock_guard l(_access); //lock guard to ensure thread safeness
    std::stringstream message;
    std::string progressBar(10, ' ');   //current progress bar

    //fill the empty progress bar according to the current percentage (_perc)
    for(int i=0; 10*(i+1) <= _perc; i+=1)
        progressBar[i] = '=';

    //write the formatted message to ostream out
    message << "\r(" << _time << ") - [" << _head << "] - " << _body << " [" << progressBar << "] " << std::setw(3) << _perc << "% - " << _tail;
    out << message.str() << std::flush;
}

/**
 * method used to update the progress percentage
 *
 * @param newPerc new percentage
 *
 * @author Michele Crepaldi s269551
 */
void Message::update(int newPerc){
    _perc = newPerc;    //update the percentage (_perc)
}

/**
 * TS_message operator<< override; used to print the message (always in a thread safe way)
 * <p>The format used is the following:</p>
 * <p>(TIME) - [HEAD] - BODY [          ] xxx% - TAIL
 *
 * @param out ostream to print to
 * @param m Message to print
 * @return ostream reference
 *
 * @author Michele Crepaldi s269551
 */
std::ostream& operator<< (std::ostream &out, Message &m){
    m.print(out);   //print the message
    return out;
}

/**
 * static function to print to ostream in a thread safe way.
 * <p>The format used is the following (if the TAIL is "" it is not printed):</p>
 * <p>(TIME) - [HEAD] - BODY - TAIL</p>
 *
 * @param out ostream to print to
 * @param head message head
 * @param body message body
 * @param tail message tail
 *
 * @author Michele Crepaldi s269551
 */
void Message::print(std::ostream &out, const std::string &head, const std::string &body, const std::string &tail) {
    std::lock_guard l(_access); //lock guard to ensure thread safeness

    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); //current time
    std::tm *gmt = std::localtime(&time);   //current time as std::tm type
    std::stringstream message;

    //compose the message
    message << "(" << std::put_time(gmt, "%F %T") << ") - [" << head << "] - " << body;

    if(!tail.empty())   //the tail is optional
        message << " - " << tail;

    //write the formatted message to ostream out
    message << std::endl;
    out << message.str();
}

/**
 * static function to print to ostream in a thread safe way. Overload with no tail.
 * <p>The format used is the following:</p>
 * <p>(TIME) - [HEAD] - BODY</p>
 *
 * @param out ostream to print to
 * @param head head of the message
 * @param body body of the message
 *
 * @author Michele Crepaldi s269551
 */
void Message::print(std::ostream &out, const std::string &head, const std::string &body) {
    print(out, head, body, "");
}