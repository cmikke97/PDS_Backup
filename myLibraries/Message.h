//
// Created by Michele Crepaldi s269551 on 04/11/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#ifndef MESSAGE_H
#define MESSAGE_H

#include <iostream>
#include <mutex>


/**
 * Message class. Simple class to print messages to an ostream in a thread safe way
 *  and to display a progress bar.
 *
 * <p>Static print methods may be used if the progress bar is not needed
 *
 * @author Michele Crepaldi s269551
 */
class Message {
public:
    Message() = default;    //default empty constructor
    Message(const Message&) = default;  //default copy constructor
    Message& operator=(const Message&) = default;   //default copy assignment
    Message(Message &&) = default;  //default move constructor
    Message& operator=(Message &&) = default;   //default move assignment

    Message(std::string head, std::string body, std::string tail);  //constructor
    void update(int newPerc);   //update the progress method

    friend std::ostream& operator<<(std::ostream &out, Message &c); //operator<< override

    //print method  (print the message, with progress bar)
    void print(std::ostream &out);

    //static print methods (to just print a message, without progress bar)
    static void print(std::ostream &out, const std::string &head, const std::string &body, const std::string &tail);
    static void print(std::ostream &out, const std::string &head, const std::string &body);

private:
    //access mutex to synchronize threads in printing to ostream
    static std::mutex _access;

    std::string _head;   //head of the message
    std::string _body;   //body of the message
    std::string _tail;   //tail of the message
    std::string _time;   //time the message was first created
    int _perc{};   //percentage of completion
};


#endif //MESSAGE_H
