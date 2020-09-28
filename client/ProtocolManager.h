//
// Created by michele on 26/09/2020.
//

#ifndef CLIENT_PROTOCOLMANAGER_H
#define CLIENT_PROTOCOLMANAGER_H


#include <string>
#include <messages.pb.h>
#include "../myLibraries/Socket.h"
#include "../Event.h"

/**
 * class used to manage the interactions with the server
 *
 * @author Michele Crepaldi
 */
class ProtocolManager {
    Socket &s;
    Database &db;
    messages::ClientMessage clientMessage;
    messages::ServerMessage serverMessage;

    std::vector<Event> waitingForResponse;
    int start, end, size, protocolVersion;

public:
    ProtocolManager(Socket &s, Database &db, int max, int ver);
    void authenticate(const std::string& username, const std::string& password);
    void quit();
    void send(Event &e);
    void composeMessage(Event &e);
    void receive();
    bool canSend() const;
    void recoverFromError();
    void sendFile(const std::filesystem::path& path, int options);
};


#endif //CLIENT_PROTOCOLMANAGER_H
